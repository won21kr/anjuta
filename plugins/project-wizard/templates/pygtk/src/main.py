[+ autogen5 template +]
#!/usr/bin/python
#
# main.py
# Copyright (C) [+Author+] [+(shell "date +%Y")+] <[+Email+]>
# 
[+CASE (get "License") +]
[+ == "BSD"  +][+(bsd  (get "Name") (get "Author") "# ")+]
[+ == "LGPL" +][+(lgpl (get "Name") (get "Author") "# ")+]
[+ == "GPL"  +][+(gpl  (get "Name")                "# ")+]
[+ESAC+]

from gi.repository import Gtk, GdkPixbuf, Gdk
import os, sys

[+IF (=(get "HaveBuilderUI") "1")+]
#Comment the first line and uncomment the second before installing
#or making the tarball (alternatively, use project variables)
UI_FILE = "src/[+NameHLower+].ui"
#UI_FILE = "/usr/local/share/[+NameHLower+]/ui/[+NameHLower+].ui"
[+ENDIF+]

class GUI:
	def __init__(self):
[+IF (=(get "HaveBuilderUI") "1")+]
		self.builder = Gtk.Builder()
		self.builder.add_from_file(UI_FILE)
		self.builder.connect_signals(self)

		window = self.builder.get_object('window')
[+ELSE+]
		window = Gtk.Window()
		window.set_title ("Hello World")
		window.connect_after('destroy', self.destroy)
[+ENDIF+]

		window.show_all()

	def destroy(window, self):
		Gtk.main_quit()

def main():
	app = GUI()
	Gtk.main()
		
if __name__ == "__main__":
    sys.exit(main())