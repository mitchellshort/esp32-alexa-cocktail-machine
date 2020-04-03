import websockets
import asyncio
import json
import time, os


class HttpWSSProtocol(websockets.WebSocketServerProtocol):
    rwebsocket = None
    rddata = None

    async def handler(self):
        try:
            #while True:
            request_line, headers = await websockets.http.read_message(self.reader)
            #print(headers)
            method, path, version = request_line[:-2].decode().split(None, 2)
            #print(self.reader)
        except Exception as e:
            #print(e.args)
            self.writer.close()
            self.ws_server.unregister(self)

            raise

        # TODO: Check headers etc. to see if we are to upgrade to WS.
        if path == '/ws':
            # HACK: Put the read data back, to continue with normal WS handling.
            self.reader.feed_data(bytes(request_line))
            self.reader.feed_data(headers.as_bytes().replace(b'\n', b'\r\n'))

            return await super(HttpWSSProtocol, self).handler()
        else:
            try:
                return await self.http_handler(method, path, version)
            except Exception as e:
                print(e)
            finally:

                self.writer.close()
                self.ws_server.unregister(self)


    async def http_handler(self, method, path, version):
        response = ''
        try :
            alexaRequest = self.reader._buffer.decode('utf-8')
            #print("Req-->"+alexaRequest)
            RequestJson = json.loads(alexaRequest)['request']['intent']['slots']   
            IDjson = json.loads(alexaRequest)['request']['requestId'] 
            ESPparameters = {}
            ESPparameters['id'] = IDjson
            if 'value' in RequestJson['cocktail_list'].keys():
                value = RequestJson['cocktail_list']['value']
                ESPparameters['value'] = value
                SynonymJson = json.loads(alexaRequest)['request']['intent']['slots']['cocktail_list']['resolutions']['resolutionsPerAuthority'][0]['values'][0]
                if 'name' not in SynonymJson['value'].keys():
                    print({"cocktail":"no synonym","cocktail_syn":value,"query":"cmd"})
                    jsonRequest = {"cocktail": "no synonym", "cocktail_syn": value, "query": "cmd"}
                else:
                    obj = SynonymJson['value']['name']
                    ESPparameters['cocktail'] = obj
                    if 'value' in RequestJson['multiplier'].keys():
                        MultiplierJson = json.loads(alexaRequest)['request']['intent']['slots']['multiplier']['resolutions']['resolutionsPerAuthority'][0]['values'][0]
                        multiplier = MultiplierJson['value']['name']
                        print({"cocktail":obj,"cocktail_syn":value,"multiplier": multiplier})
                        ESPparameters['multiplier'] = multiplier
                        jsonRequest = {"cocktail": obj, "cocktail_syn": value, "multiplier": multiplier}
                    else:
                        print({"cocktail":obj,"cocktail_syn":value,"multiplier":"cmd"})
                        jsonRequest = {"cocktail": obj, "cocktail_syn": value, "multiplier": "cmd"}  
                        ESPparameters['cmd'] = 'cmd'
            
                            
            #with open('data.json', 'w') as outfile:
            #    json.dump(json.dumps(jsonRequest), outfile)
                #await self.rwebsocket.send(alexaRequest)            
                # # send command to ESP over websocket
            if self.rwebsocket== None:
                print("Device is not connected!")
                return
            #await self.rwebsocket.send(json.dumps(googleRequestJson))
            print('sending data')
            await self.rwebsocket.send(json.dumps(ESPparameters))
            print('waiting for response')
            #wait for response and send it back to Alexa as is
            #self.rddata = await self.rwebsocket.recv()
            
            #print('recieved: ' + self.rddata)
            
            response = '\r\n'.join([
                'HTTP/1.1 200 OK',
                'Content-Type: text/json',
                '',
                '{\"version\": \"1.0\",\"sessionAttributes\": {},\"response\": {\"outputSpeech\": {\"type\": \"PlainText\",\"text\": \"Ok, doing that now"},\"shouldEndSession\": true}}',
            ])
            print('response: ' + response)
        except Exception as e:
            print(e)

        self.writer.write(response.encode())



def updateData(data):
    HttpWSSProtocol.rddata = data

async def ws_handler(websocket, path):
    game_name = 'g1'
    try:
        with open('data.json') as data_file:
            data = json.load(data_file)
        HttpWSSProtocol.rwebsocket = websocket
        await websocket.send(data)
        data ='{"empty":"empty"}'
        while True:
            data = await websocket.recv()
            updateData(data)
    except Exception as e:
        print(e)
    finally:
        print("")

def _read_ready(self):
    if self._conn_lost:
        return
    try:
        time.sleep(.10)
        data = self._sock.recv(self.max_size)
    except (BlockingIOError, InterruptedError):
        pass
    except Exception as exc:
        self._fatal_error(exc, 'Fatal read error on socket transport')
    else:
        if data:
            self._protocol.data_received(data)
        else:
            if self._loop.get_debug():
                print("%r received EOF")
            keep_open = self._protocol.eof_received()
            if keep_open:
                # We're keeping the connection open so the
                # protocol can write more, but we still can't
                # receive more, so remove the reader callback.
                self._loop._remove_reader(self._sock_fd)
            else:
                self.close()

asyncio.selector_events._SelectorSocketTransport._read_ready = _read_ready

port = int(os.getenv('PORT', 80))#5687
start_server = websockets.serve(ws_handler, '', port, klass=HttpWSSProtocol)
# logger.info('Listening on port %d', port)

asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()
