package server

import (
	"fmt"
	"net/http"
	"os"
	"path/filepath"
)

type Server struct {
	rootDir string
	port    int
}

func New(rootDir string, port int) *Server {
	return &Server{
		rootDir: rootDir,
		port:    port,
	}
}

func (s *Server) Start() error {
	// Verify directory exists
	if _, err := os.Stat(s.rootDir); os.IsNotExist(err) {
		return fmt.Errorf("directory does not exist: %s", s.rootDir)
	}

	// Create file server with CORS middleware
	fileServer := http.FileServer(http.Dir(s.rootDir))
	handler := corsMiddleware(fileServer)

	addr := fmt.Sprintf(":%d", s.port)
	fmt.Printf("Serving %s on http://127.0.0.1%s\n", s.rootDir, addr)
	fmt.Println("Press Ctrl+C to stop")

	return http.ListenAndServe(addr, handler)
}

func corsMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// Set CORS headers for local development
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, HEAD, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Range")
		w.Header().Set("Access-Control-Expose-Headers", "Content-Length, Content-Range")

		// Handle preflight requests
		if r.Method == "OPTIONS" {
			w.WriteHeader(http.StatusOK)
			return
		}

		// Log requests
		fmt.Printf("%s %s\n", r.Method, r.URL.Path)

		next.ServeHTTP(w, r)
	})
}

func FindGameBinDir() (string, error) {
	// Get the executable's directory to find project root
	exePath, err := os.Executable()
	if err != nil {
		return "", fmt.Errorf("failed to get executable path: %w", err)
	}

	// Navigate from pandora/tools/forge/src/ to project root
	forgeDir := filepath.Dir(exePath)
	projectRoot := filepath.Join(forgeDir, "..", "..", "..", "..")
	projectRoot, err = filepath.Abs(projectRoot)
	if err != nil {
		return "", fmt.Errorf("failed to get absolute path: %w", err)
	}

	gameBinDir := filepath.Join(projectRoot, "game", "bin")
	return gameBinDir, nil
}
