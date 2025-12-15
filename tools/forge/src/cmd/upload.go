package cmd

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"

	"wings_of_steel/forge/upload"
)

var UploadCmd = &cobra.Command{
	Use:   "upload",
	Short: "Upload assets to Cloudflare R2",
	Long: `Upload game assets to Cloudflare R2 via the Forge worker.

Reads the manifest from pandora/tools/forge/bin/cache/manifest.json
and uploads each file to the Cloudflare Worker endpoint.

Run 'forge manifest' first to generate the manifest.`,
	Run: func(cmd *cobra.Command, args []string) {
		manifestPath, assetsDir, err := upload.FindPaths()
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}

		// Check manifest exists
		if _, err := os.Stat(manifestPath); os.IsNotExist(err) {
			fmt.Fprintf(os.Stderr, "Error: manifest not found at %s\n", manifestPath)
			fmt.Fprintf(os.Stderr, "Run 'forge manifest' first to generate it.\n")
			os.Exit(1)
		}

		fmt.Printf("Manifest: %s\n", manifestPath)
		fmt.Printf("Assets:   %s\n\n", assetsDir)

		uploader := upload.NewUploader(manifestPath, assetsDir)
		if err := uploader.Upload(); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
	},
}
