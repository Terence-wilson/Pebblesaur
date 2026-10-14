var App = require('./app').App;
var config = require('./config');
var liftosaur = require('./liftosaur');
var messenger = require('./messenger');

var app = new App({
  storage: localStorage,
  send: messenger.send,
  sendAll: messenger.sendAll,
  createClient: function (options) {
    return new liftosaur.Client(options);
  },
  setInterval: function (fn, ms) {
    return setInterval(fn, ms);
  },
  clearInterval: function (id) {
    clearInterval(id);
  }
});

Pebble.addEventListener('ready', function () {
  app.onReady();
});

Pebble.addEventListener('appmessage', function (e) {
  app.onAppMessage(e.payload);
});

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(config.configPage(app.hasApiKey(), 'pebblejs://close#'));
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) {
    return;
  }
  var settings;
  try {
    settings = JSON.parse(decodeURIComponent(e.response));
  } catch (err) {
    console.log('Bad settings response: ' + e.response);
    return;
  }
  app.onConfig(settings || {});
});
