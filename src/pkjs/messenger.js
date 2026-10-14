// Sends AppMessages to the watch one at a time, retrying failures. The watch
// can only take one message in flight, and a workout spans several.

var MAX_RETRIES = 3;
var RETRY_DELAY_MS = 500;

var queue = [];
var sending = false;

function sendNext() {
  if (sending || queue.length === 0) {
    return;
  }
  sending = true;
  var item = queue[0];
  Pebble.sendAppMessage(item.message, function () {
    queue.shift();
    sending = false;
    sendNext();
  }, function () {
    sending = false;
    item.attempts++;
    if (item.attempts > MAX_RETRIES) {
      console.log('Dropping message after retries: ' + JSON.stringify(item.message));
      queue.shift();
    }
    setTimeout(sendNext, RETRY_DELAY_MS);
  });
}

function send(message) {
  queue.push({ message: message, attempts: 0 });
  sendNext();
}

function sendAll(messages) {
  messages.forEach(send);
}

module.exports = {
  send: send,
  sendAll: sendAll
};
