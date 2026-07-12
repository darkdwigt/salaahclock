#include "web_ui.h"
#include "ai_query.h"
#include <ESPmDNS.h>
#include <WebServer.h>

static WebServer server(80);
static String pendingAnswer;
static bool hasAnswer = false;

// Voice capture happens in the browser, not on the ESP32 - the board has
// no microphone. Two paths, both client-side:
//   1. Web Speech API auto-listen button - works on desktop Chrome/Edge.
//      Unavailable on iOS in ANY browser (Apple requires all iOS browsers
//      to use WebKit, which doesn't implement this API) - hidden there.
//   2. Text box you dictate into using the phone's own keyboard mic icon
//      (iOS/Android OS-level dictation, not a web API) then tap Send.
//      This is the only path that works on iOS.
static const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>Ask the Salaah Clock</title>
<style>
body{font-family:sans-serif;text-align:center;padding:2em;background:#111;color:#eee}
button{font-size:1.2em;padding:0.6em 1.2em;border-radius:10px;border:none;background:#2a6e2a;color:#fff}
button:active{background:#1e4f1e}
input{font-size:1.1em;padding:0.5em;width:80%;max-width:20em;border-radius:8px;border:1px solid #444;background:#222;color:#eee;margin-top:1em}
#s{color:#999;margin-top:1.2em}
#a{margin-top:0.6em;white-space:pre-wrap;font-size:1.1em}
#hint{color:#777;font-size:0.85em;margin-top:0.4em}
</style></head><body>
<h2>Ask the Salaah Clock</h2>
<button id=b onclick=listen()>Tap and speak</button>
<div><input id=t type=text placeholder="or type/dictate here" onkeydown="if(event.key=='Enter')send(t.value)">
<div><button onclick="send(t.value)">Send</button></div>
<div id=hint>On iPhone: tap the mic icon on the keyboard to dictate into the box.</div></div>
<p id=s></p><p id=a></p>
<script>
var R=window.SpeechRecognition||window.webkitSpeechRecognition;
if(!R){document.getElementById('b').style.display='none';}
function listen(){
  var r=new R();r.lang='en-US';
  r.onresult=function(e){send(e.results[0][0].transcript);};
  r.onerror=function(e){document.getElementById('s').textContent='Mic error: '+e.error;};
  r.start();
}
function send(q){
  q=(q||'').trim();
  if(!q)return;
  document.getElementById('s').textContent='You asked: '+q;
  document.getElementById('a').textContent='Thinking...';
  fetch('/ask',{method:'POST',body:q})
    .then(function(res){return res.text();})
    .then(function(t){document.getElementById('a').textContent=t;})
    .catch(function(){document.getElementById('a').textContent='Could not reach the clock.';});
}
</script></body></html>
)HTML";

static void handleRoot() {
    server.send_P(200, "text/html", PAGE);
}

static void handleAsk() {
    String question = server.arg("plain");
    question.trim();
    if (question.length() == 0) {
        server.send(400, "text/plain", "No question received.");
        return;
    }

    Serial.printf("Voice question: %s\n", question.c_str());
    String answer;
    if (askAI(question, answer)) {
        pendingAnswer = answer;
        hasAnswer = true;
        server.send(200, "text/plain", answer);
    } else {
        server.send(200, "text/plain", "Sorry, couldn't reach the AI right now.");
    }
}

void webUIInit() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/ask", HTTP_POST, handleAsk);
    server.begin();

    if (MDNS.begin("salaahclock")) {
        Serial.println("Voice page: http://salaahclock.local/");
    } else {
        Serial.println("mDNS start failed - use the IP printed above instead.");
    }
}

void webUIHandle() {
    server.handleClient();
}

bool webUITakeAnswer(String &answer) {
    if (!hasAnswer) return false;
    answer = pendingAnswer;
    hasAnswer = false;
    return true;
}
