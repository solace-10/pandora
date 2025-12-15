package cmd

import (
	"fmt"

	"github.com/spf13/cobra"
)

var manifestOutput string

var ManifestCmd = &cobra.Command{
	Use:   "manifest",
	Short: "Generate asset manifest",
	Long: `Generate a manifest.json file from the game assets.

The manifest contains the path, XXHash3-64 hash, and size of each asset file.
This manifest is used by the web build to download and verify assets.`,
	Run: func(cmd *cobra.Command, args []string) {
		fmt.Println("Generating manifest...")
		fmt.Printf("Output: %s\n", manifestOutput)
		// TODO: Implement manifest generation
		fmt.Println("TODO: Walk game/bin/data/core/, compute hashes, write JSON")
	},
}

func init() {
	ManifestCmd.Flags().StringVarP(&manifestOutput, "output", "o", "game/bin/data/core/manifest.json", "Output path for manifest file")
}
