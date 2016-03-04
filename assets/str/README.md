Text-to-speech system
=====================

1. Run `./text_to_speech` (or `text_to_speech.bat` if you're on Windows).
This script goes through all the strings in the `soren` string category in 
`en.json` and converts them to wav files via the IBM Bluemix text-to-speech
API.

2. Open the Wwise project.

3. Delete all the `soren` events and SFX.

4. Right-click the `soren` Actor-Mixer and hit "Import audio files". Import
all the soren audio files as SFX.

5. Select all the SFX, right-click and hit "New event" -> "One Event per Object"

6. Put the created events in the `soren` folder.

7. Select all the events, right-click, and hit "Batch rename".

8. Replace "Play_" with "".
