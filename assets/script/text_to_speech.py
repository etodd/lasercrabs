import urllib.request
import sys
import json
import os
import wave
import io

# parses a strings file, compares it to a cached version of the file from the last run
# if there are any differences, it converts them to wav files via the IBM bluemix API
# and saves them in the audio directory

arg_start = sys.argv.index('--') + 1
username = sys.argv[arg_start + 0]
password = sys.argv[arg_start + 1]
string_path = 'en.json'
cache_path = 'soren.cache'

with open(string_path, 'r') as string_file:
	strings = json.loads(string_file.read())['soren']

cache = {}
try:
	with open(cache_path, 'r') as cache_file:
		cache = json.loads(cache_file.read())
except:
	pass

audio_directory = '../audio/Originals/SFX/soren/'

def key_to_audio_path(key):
	return os.path.join(audio_directory, '{}.wav'.format(key))

for key in cache:
	if key not in strings: # been deleted
		try:
			os.remove(key_to_audio_path(key))
		except:
			pass

auth_handler = urllib.request.HTTPBasicAuthHandler()
auth_handler.add_password(realm = 'IBM Watson Gateway Log-in', uri = 'https://stream.watsonplatform.net/text-to-speech/api/v1/synthesize', user = username, passwd = password)
opener = urllib.request.build_opener(auth_handler)
urllib.request.install_opener(opener)

for key in strings:
	value = strings[key]
	cached_value = cache.get(key, None)
	if value != cached_value:
		req = urllib.request.Request(
			url = 'https://stream.watsonplatform.net/text-to-speech/api/v1/synthesize?accept=audio/wav&voice=en-US_AllisonVoice',
			data = json.dumps({ 'text': value }).encode('utf-8'),
			method = 'POST'
		)
		req.add_header('Content-Type', 'application/json')

		with urllib.request.urlopen(req) as response:
			with wave.open(io.BytesIO(response.read()), 'rb') as old:
				# the Bluemix text-to-speech API outputs streaming wav files
				# which have incorrect sizes in the headers
				# we have to fix the sizes ourselves
				old._data_chunk.file.seek(0, os.SEEK_END)
				filesize = old._data_chunk.file.tell()
				old._data_chunk.chunksize = filesize - old._data_chunk.offset
				old._nframes = old._data_chunk.chunksize // old._framesize
				old.setpos(0)
				frames = old.readframes(old.getnframes())
				params = old.getparams()

		with open(key_to_audio_path(key), 'wb') as out_file:
			with wave.open(out_file, 'wb') as new:
				new.setparams(params)
				new.writeframes(frames)

		print(key)

# save cache
with open(cache_path, 'w') as cache_file:
	cache_file.write(json.dumps(strings))
