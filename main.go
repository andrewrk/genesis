package main

import (
	"github.com/mattn/go-gtk/gtk"
	"os"
)

func main() {
	gtk.Init(&os.Args)
	window := gtk.NewWindow(gtk.WINDOW_TOPLEVEL)
	window.SetTitle("Hello world")
	window.Connect("destroy", gtk.MainQuit)

	window.SetSizeRequest(400, 400)
	window.ShowAll()

	gtk.Main()
}
