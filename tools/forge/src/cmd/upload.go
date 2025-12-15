package cmd

import (
	"fmt"

	"github.com/spf13/cobra"
)

var uploadForce bool

var UploadCmd = &cobra.Command{
	Use:   "upload",
	Short: "Upload assets to Cloudflare R2",
	Long: `Upload game assets to a Cloudflare R2 bucket.

Required environment variables:
  R2_ACCESS_KEY_ID     - R2 API token access key
  R2_SECRET_ACCESS_KEY - R2 API token secret
  R2_ACCOUNT_ID        - Cloudflare account ID
  R2_BUCKET            - R2 bucket name

By default, only changed files are uploaded (based on hash comparison).
Use --force to upload all files regardless of changes.`,
	Run: func(cmd *cobra.Command, args []string) {
		fmt.Println("Uploading to R2...")
		fmt.Printf("Force upload: %v\n", uploadForce)
		// TODO: Implement R2 upload
		fmt.Println("TODO: Connect to R2, upload files from game/bin/data/core/")
	},
}

func init() {
	UploadCmd.Flags().BoolVarP(&uploadForce, "force", "f", false, "Upload all files regardless of changes")
}
