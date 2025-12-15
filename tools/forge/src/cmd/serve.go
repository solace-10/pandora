package cmd

import (
	"fmt"

	"github.com/spf13/cobra"
)

var servePort int
var serveDir string

var ServeCmd = &cobra.Command{
	Use:   "serve",
	Short: "Run local HTTP server",
	Long: `Start a local HTTP server to serve game assets for development.

The server includes CORS headers for local development and serves
files from the game/bin directory by default.`,
	Run: func(cmd *cobra.Command, args []string) {
		fmt.Printf("Starting server on port %d...\n", servePort)
		fmt.Printf("Serving directory: %s\n", serveDir)
		// TODO: Implement HTTP server
		fmt.Println("TODO: Start HTTP server with CORS headers")
	},
}

func init() {
	ServeCmd.Flags().IntVarP(&servePort, "port", "p", 8000, "Port to serve on")
	ServeCmd.Flags().StringVarP(&serveDir, "dir", "d", "game/bin", "Directory to serve")
}
