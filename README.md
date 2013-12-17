non
===
This branch was created originally to optimize graphics drawing in Non Sequencer and has since become more general in scope to also include bug fixes and functional improvements.

About Non
===
Non reinvents the DAW.  Powerful enough to form a complete studio, fast and light enough to run on low-end hardware like the eeePC or Raspberry Pi, and so reliable that it can be used live, the Non DAW Studio is a modular system composed of four main parts: Non Timeline, a non-destructive, non-linear audio recorder and arranger. Non Mixer, a live mixer with effects plugin hosting and advanced Ambisonics spatialization control. Non Sequencer, a live, pattern based MIDI sequencer, and finally, the Non Session Manager to tie together these applications and more into cohesive song-level units.


Documentation
===
Here you will find some Non Sequencer user interface documentation as I figure out how to use it.  This will include un-official improvements that might not be in the main non branch, though I'll try to indicate them with a star (*).

Pattern Editor
===
* Left click to place a note
* Left click on a note and drag vertically to change velocity.
* Left click on a note and drag horizontally to change note length.
* Right mouse drag in grid to selection a box region.
* Left click drag horizontally on ruler to select range.
* CTRL left click on a note to toggle selection of it.
* Hold down CTRL while selecting a range/region to add to selection (*).
* Notes will be pasted to last range selection or the beginning of the pattern if no selection.

Key Bindings
===
* Ctrl-Delete: Delete time
* Ctrl-Insert: Insert time
* Ctrl-Right: Pan to next note
* Ctrl-Left: Pan to previous note
* Left: Pan left
* Right: Pan right
* Up: Pan up
* Down: Pan down
* f: Pan to playhead
* r: Select range
* q/CTRL-SHIFT-A: Select none
* CTRL-A: Select all
* i: Invert selection
* <: Move selected left
* >: Move selected right
* ,: Move selected up
* .: Move selected down
* C: Crop
* d: Duplicate Pattern/Sequence
* D: Duplicate range
* t: Trim to last note
* CTRL-C: Copy
* CTRL-V: Paste
* CTRL-X: Cut
