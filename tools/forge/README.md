# Forge

Forge is the asset pipeline tool for Pandora. It compiles source assets into
their shipping form, hashes them into a manifest, and uploads them to
Cloudflare R2 for web builds.

## Commands

| Command | Purpose |
|---|---|
| `forge assets` | Compile `game/assets` into `game/bin/data/core` |
| `forge manifest` | Hash `game/bin/data/core` into `game/bin/manifest.json` |
| `forge upload` | Upload the manifest's files to Cloudflare R2 |
| `forge serve` | Serve `game/bin` over HTTP for local development |

CMake runs `forge assets` on every build, and additionally runs `manifest` and
`upload` for web builds. Assets are cached, so a build with nothing to do costs
close to nothing.

## Asset descriptors

Every `*.asset.json` file under `game/assets` describes one asset. `source` is
relative to `game/assets`, and `destination` is relative to
`game/bin/data/core` — which is what the game sees through the VFS, so a
destination of `fonts/UbuntuMono-R.ttf` is loaded as `/fonts/UbuntuMono-R.ttf`.

```json
{
    "source": "fonts/Ubuntu/UbuntuMono-R.ttf",
    "destination": "fonts/UbuntuMono-R.ttf"
}
```

| Field | Required | Meaning |
|---|---|---|
| `source` | yes | Input file, relative to `game/assets` |
| `destination` | no | Output file, relative to `game/bin/data/core`. Defaults to the source path with the tool's output extension |
| `tool` | no | Overrides the tool chosen from the source's extension |

Unrecognised fields are an error rather than being ignored, so a typo fails the
build instead of silently shipping a wrongly compiled asset.

### Tools

The tool is picked from the source's extension. Anything without a dedicated
tool is copied verbatim.

#### `copy`

Copies the source unchanged. Takes no options.

#### `texture`

Handles `.png`, `.jpg` and `.jpeg`, and always produces a `.ktx2`. It encodes
with the `ktx` tool from KTX-Software, vendored in `bin/ktx`, so nothing needs
installing and every machine encodes identically. See
[bin/ktx/README.md](bin/ktx/README.md) for the version, the platforms covered,
and how to update it.

```json
{
    "source": "textures/earth.png",
    "destination": "textures/earth.ktx2",
    "width": 2048,
    "usage": "color"
}
```

| Option | Default | Meaning |
|---|---|---|
| `width`, `height` | source size | Resamples the image. Giving only one preserves the aspect ratio |
| `usage` | `color` | How many channels the texture keeps and what format it ends up in. See below |
| `encoding` | `none` | `uastc` or `basis-lz` to apply GPU block compression. Only for `color` and `linear` |
| `zstd` | none | Supercompression level, 1 to 22 |

### Usage

`usage` is the field that decides everything about the output format, so an
impossible combination cannot be written down - there is no sRGB single channel
format on any GPU, for instance.

| `usage` | Channels | Ships as | Use for |
|---|---|---|---|
| `color` | 4, sRGB | Basis payload, transcoded on load | Albedo, emissive, anything the eye reads as colour |
| `linear` | 4, linear | Basis payload, transcoded on load | Four channels of data rather than colour |
| `mask` | 1, linear | `BC4_UNORM` directly | Coverage, gloss, AO, height - anything greyscale |
| `normal` | 2, linear | `BC5_UNORM` directly | Tangent space normal maps |

`mask` and `normal` do not take an `encoding`, because Basis is the wrong
transport for them and they always produce the format above. UASTC is
structurally a four channel format that spends eight bits per texel whatever it
is given, so it cannot represent a mask any more cheaply than a colour map -
declaring a single channel source changes the file's channel marker and not one
byte of its payload. Storing the block format directly is both smaller on disk
and half the memory on the GPU. Forge gets there by encoding to UASTC and then
transcoding, since `ktx create` cannot write a BC format but `ktx transcode` can.

`normal` discards Z, which a tangent space normal map does not need: the engine
reconstructs it as `sqrt(1 - x*x - y*y)`, which also guarantees a unit normal.
BC5 is the only sensible target - it gives X and Y a dedicated channel each,
where BC7 would share endpoints across RGB and put Y in alpha.

### Encoding, for the four channel usages

`encoding` and `zstd` default to off, which produces an uncompressed RGBA8 file.
That is larger than the source PNG or JPEG, so anything shipping should set them.

The engine transcodes a Basis Universal payload at load time to whatever block
format the device supports, so one encoded file serves every platform. `uastc`
keeps more quality and is the right choice for detailed colour maps; `basis-lz`
produces far smaller files and suits textures where that matters more.

Pair `uastc` with `zstd` - it relies on it, being otherwise a fixed byte per
texel. Do **not** pair `basis-lz` with `zstd`: BasisLZ carries its own entropy
coding, `ktx` rejects the combination outright, and Forge now fails the
descriptor before spending any time on it.

Note that block compression needs dimensions that are a multiple of 4. The engine
falls back to uncompressed for a texture that is not, having logged a warning, so
set `width` and `height` accordingly. A `normal` texture is refused outright
instead, as the uncompressed fallback would move Y into alpha.

Also note that `ktx` refuses to resample any source larger than 16384 px, so an
image above that has to be reduced before it reaches `game/assets`.

## Caching

Compiled artifacts are cached in `.forge-cache` at the project root, keyed by a
hash of the source's contents, the descriptor's options, the destination, and
the encoder's version. Changing any of them recompiles the asset; changing them
back restores the previous artifact without recompiling. Because the encoder's
version is part of the key, updating the vendored `ktx` re-encodes every
texture rather than leaving a mix of old and new artifacts behind.

The cache sits outside `build/` on purpose, so wiping the build directory does
not force every texture to be encoded again.

`.forge-cache/index.json` records which files in `data/core` Forge produced.
That record is what lets compiled and hand-placed assets share one directory:
when a descriptor is deleted, Forge removes the file it wrote, and never
touches anything it did not write. A generated file that was edited by hand is
left in place and reported rather than deleted.

## Flags

`forge assets` takes `--force` to ignore the cache, `--clean` to remove
compiled assets and the cache, `--jobs` to set concurrency, and `--verbose` to
also list assets that were already up to date.

## Adding a tool

Add a file to `src/assets/` with a type implementing `Tool` and an `init()`
that calls `Register` with the extensions it handles. Nothing else needs to
change.

`Fingerprint` must capture everything that affects the output, including the
version of any external encoder invoked, otherwise upgrading that encoder will
leave stale artifacts in the cache.
