package assets

import (
	"bytes"
	"fmt"
	"image"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"

	// Registers the decoders used to read source dimensions.
	_ "image/jpeg"
	_ "image/png"

	"wings_of_steel/forge/paths"
)

func init() {
	Register(&textureTool{}, ".png", ".jpg", ".jpeg")
}

// ktxVendorDocs points at the instructions for adding a platform's encoder.
const ktxVendorDocs = "pandora/tools/forge/bin/ktx/README.md"

// textureOptions are the descriptor fields the texture tool understands.
type textureOptions struct {
	// Width and Height resize the source. Giving only one of them preserves
	// the aspect ratio.
	Width  int `json:"width"`
	Height int `json:"height"`

	// Usage decides the colour space the texture is encoded in. Textures that
	// carry data rather than colour - normal maps, roughness, masks - must be
	// linear, or their values are gamma-mangled on load.
	Usage string `json:"usage"`

	// Encoding optionally applies GPU block compression. It defaults to none,
	// as nothing in the engine transcodes yet.
	Encoding string `json:"encoding"`

	// Zstd optionally applies supercompression at the given level.
	Zstd int `json:"zstd"`
}

const (
	usageColor  = "color"
	usageNormal = "normal"
	usageLinear = "linear"
)

type textureTool struct{}

func (t *textureTool) Name() string {
	return "texture"
}

func (t *textureTool) OutputExtension(srcExt string) string {
	return ".ktx2"
}

func (t *textureTool) Fingerprint(d *Descriptor) (string, error) {
	options, err := parseTextureOptions(d)
	if err != nil {
		return "", err
	}

	// The encoder version is part of the fingerprint so that upgrading
	// KTX-Software invalidates every texture, rather than leaving a mix of
	// old and new artifacts in the cache.
	version, err := ktxVersion()
	if err != nil {
		return "", err
	}

	return fmt.Sprintf("texture/1 %s w=%d h=%d usage=%s encoding=%s zstd=%d",
		version, options.Width, options.Height, options.Usage, options.Encoding, options.Zstd), nil
}

func (t *textureTool) Compile(job *Job) error {
	options, err := parseTextureOptions(job.Descriptor)
	if err != nil {
		return err
	}

	executable, err := ktxExecutable()
	if err != nil {
		return err
	}

	arguments := []string{"create", "--generate-mipmap"}

	// --assign-tf labels the transfer function without touching the samples,
	// which is what is wanted here: the sources are already sRGB encoded, so a
	// linear usage is a statement that the bytes are data rather than colour
	// and must not be gamma decoded.
	if options.Usage == usageColor {
		arguments = append(arguments, "--format", "R8G8B8A8_SRGB", "--assign-tf", "srgb")
	} else {
		arguments = append(arguments, "--format", "R8G8B8A8_UNORM", "--assign-tf", "linear")
	}

	// ktx resamples to whichever dimensions are given, but it does not infer
	// the missing one, so the aspect ratio is worked out here.
	if options.Width > 0 || options.Height > 0 {
		width, height, err := targetSize(job.SourcePath, options)
		if err != nil {
			return err
		}
		arguments = append(arguments, "--width", strconv.Itoa(width), "--height", strconv.Itoa(height))
	}

	if options.Encoding != "" && options.Encoding != "none" {
		arguments = append(arguments, "--encode", options.Encoding)
	}

	if options.Zstd > 0 {
		arguments = append(arguments, "--zstd", strconv.Itoa(options.Zstd))
	}

	arguments = append(arguments, job.SourcePath, job.OutputPath)

	command := exec.Command(executable, arguments...)
	var stderr bytes.Buffer
	command.Stderr = &stderr

	if err := command.Run(); err != nil {
		message := strings.TrimSpace(stderr.String())
		if message == "" {
			message = err.Error()
		}
		return fmt.Errorf("ktx create failed: %s", message)
	}

	return nil
}

func parseTextureOptions(d *Descriptor) (textureOptions, error) {
	options := textureOptions{Usage: usageColor}
	if err := d.DecodeOptions(&options); err != nil {
		return options, err
	}

	switch options.Usage {
	case usageColor, usageNormal, usageLinear:
	default:
		return options, fmt.Errorf("unknown usage '%s' (expected '%s', '%s' or '%s')",
			options.Usage, usageColor, usageNormal, usageLinear)
	}

	switch options.Encoding {
	case "", "none", "uastc", "basis-lz":
	default:
		return options, fmt.Errorf("unknown encoding '%s' (expected 'none', 'uastc' or 'basis-lz')", options.Encoding)
	}

	if options.Width < 0 || options.Height < 0 {
		return options, fmt.Errorf("width and height must not be negative")
	}

	if options.Zstd < 0 || options.Zstd > 22 {
		return options, fmt.Errorf("zstd level must be between 0 and 22, got %d", options.Zstd)
	}

	return options, nil
}

// targetSize resolves the requested dimensions, reading the source's own size
// to fill in whichever one was left out.
func targetSize(sourcePath string, options textureOptions) (int, int, error) {
	if options.Width > 0 && options.Height > 0 {
		return options.Width, options.Height, nil
	}

	file, err := os.Open(sourcePath)
	if err != nil {
		return 0, 0, fmt.Errorf("failed to read the source image: %w", err)
	}
	defer file.Close()

	config, _, err := image.DecodeConfig(file)
	if err != nil {
		return 0, 0, fmt.Errorf("failed to read the source image dimensions: %w", err)
	}

	width := options.Width
	height := options.Height

	if width > 0 {
		height = config.Height * width / config.Width
	} else {
		width = config.Width * height / config.Height
	}

	if width < 1 {
		width = 1
	}
	if height < 1 {
		height = 1
	}

	return width, height, nil
}

var (
	ktxOnce        sync.Once
	ktxPath        string
	ktxPathErr     error
	ktxVersionOnce sync.Once
	ktxVersionText string
	ktxVersionErr  error
)

// ktxExecutable returns the encoder vendored alongside Forge for this
// platform.
//
// It is the only encoder Forge will use. Textures are then guaranteed to be
// encoded identically everywhere, rather than depending on what each machine
// happens to have installed.
func ktxExecutable() (string, error) {
	ktxOnce.Do(func() {
		binDir, err := paths.ForgeBinDir()
		if err != nil {
			ktxPathErr = err
			return
		}

		name := "ktx"
		if runtime.GOOS == "windows" {
			name = "ktx.exe"
		}

		path := filepath.Join(binDir, "ktx", runtime.GOOS+"-"+runtime.GOARCH, name)
		if _, err := os.Stat(path); err != nil {
			ktxPathErr = fmt.Errorf("no ktx encoder is vendored for %s/%s: expected it at %s (see %s)",
				runtime.GOOS, runtime.GOARCH, path, ktxVendorDocs)
			return
		}

		ktxPath = path
	})

	return ktxPath, ktxPathErr
}

func ktxVersion() (string, error) {
	ktxVersionOnce.Do(func() {
		executable, err := ktxExecutable()
		if err != nil {
			ktxVersionErr = err
			return
		}

		output, err := exec.Command(executable, "--version").CombinedOutput()
		if err != nil {
			ktxVersionErr = fmt.Errorf("failed to query the ktx version: %w", err)
			return
		}

		ktxVersionText = strings.TrimSpace(string(output))
	})

	return ktxVersionText, ktxVersionErr
}
