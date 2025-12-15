package upload

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"time"

	"github.com/schollz/progressbar/v3"
)

const WorkerURL = "https://forge-cloudflare-worker.wings-of-steel.workers.dev"

type ManifestEntry struct {
	Path string `json:"path"`
	Hash string `json:"hash"`
	Size int64  `json:"size"`
}

type Uploader struct {
	manifestPath string
	assetsDir    string
	client       *http.Client
}

func NewUploader(manifestPath, assetsDir string) *Uploader {
	return &Uploader{
		manifestPath: manifestPath,
		assetsDir:    assetsDir,
		client: &http.Client{
			Timeout: 5 * time.Minute,
		},
	}
}

func (u *Uploader) Upload() error {
	// Read manifest
	manifestData, err := os.ReadFile(u.manifestPath)
	if err != nil {
		return fmt.Errorf("failed to read manifest: %w", err)
	}

	var entries []ManifestEntry
	if err := json.Unmarshal(manifestData, &entries); err != nil {
		return fmt.Errorf("failed to parse manifest: %w", err)
	}

	// Calculate total size for progress
	var totalSize int64
	for _, entry := range entries {
		totalSize += entry.Size
	}

	fmt.Printf("Uploading %d files (%.2f MB total)\n\n", len(entries), float64(totalSize)/(1024*1024))

	// Create progress bar
	bar := progressbar.NewOptions64(
		totalSize,
		progressbar.OptionSetDescription("Uploading"),
		progressbar.OptionSetWidth(40),
		progressbar.OptionShowBytes(true),
		progressbar.OptionShowCount(),
		progressbar.OptionSetTheme(progressbar.Theme{
			Saucer:        "=",
			SaucerHead:    ">",
			SaucerPadding: " ",
			BarStart:      "[",
			BarEnd:        "]",
		}),
	)

	// Upload each file
	var uploadedCount int
	var failedCount int

	for _, entry := range entries {
		filePath := filepath.Join(u.assetsDir, filepath.FromSlash(entry.Path))

		err := u.uploadFile(filePath, entry.Hash)
		if err != nil {
			fmt.Printf("\nFailed to upload %s: %v\n", entry.Path, err)
			failedCount++
		} else {
			uploadedCount++
		}

		bar.Add64(entry.Size)
	}

	fmt.Printf("\n\nUpload complete: %d succeeded, %d failed\n", uploadedCount, failedCount)

	if failedCount > 0 {
		return fmt.Errorf("%d files failed to upload", failedCount)
	}

	return nil
}

func (u *Uploader) uploadFile(filePath, hash string) error {
	// Read file
	data, err := os.ReadFile(filePath)
	if err != nil {
		return fmt.Errorf("failed to read file: %w", err)
	}

	// Create PUT request
	url := fmt.Sprintf("%s/%s", WorkerURL, hash)
	req, err := http.NewRequest(http.MethodPut, url, bytes.NewReader(data))
	if err != nil {
		return fmt.Errorf("failed to create request: %w", err)
	}

	req.Header.Set("Content-Type", "application/octet-stream")

	// Send request
	resp, err := u.client.Do(req)
	if err != nil {
		return fmt.Errorf("request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		body, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("server returned %d: %s", resp.StatusCode, string(body))
	}

	return nil
}

func FindPaths() (manifestPath, assetsDir string, err error) {
	// Get the executable's directory to find project root
	exePath, err := os.Executable()
	if err != nil {
		return "", "", fmt.Errorf("failed to get executable path: %w", err)
	}

	// Navigate from pandora/tools/forge/src/ to project root
	forgeDir := filepath.Dir(exePath)
	projectRoot := filepath.Join(forgeDir, "..", "..", "..", "..")
	projectRoot, err = filepath.Abs(projectRoot)
	if err != nil {
		return "", "", fmt.Errorf("failed to get absolute path: %w", err)
	}

	manifestPath = filepath.Join(projectRoot, "pandora", "tools", "forge", "bin", "cache", "manifest.json")
	assetsDir = filepath.Join(projectRoot, "game", "bin", "data", "core")

	return manifestPath, assetsDir, nil
}
