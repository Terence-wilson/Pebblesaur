var test = require('node:test');
var assert = require('node:assert');
var App = require('../src/pkjs/app').App;
var protocol = require('../src/pkjs/protocol');
var fixture = require('./fixtures/workout.json');

var MsgType = protocol.MsgType;
var Action = protocol.Action;

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

// A fake Liftosaur that answers synchronously and records every call.
function FakeLiftosaur() {
  this.current = null;
  this.next = clone(fixture);
  this.calls = [];
  this.failNext = null;
}

FakeLiftosaur.prototype.client = function () {
  var server = this;
  function reply(name, args, callback, result) {
    server.calls.push({ name: name, args: args });
    if (server.failNext) {
      var err = server.failNext;
      server.failNext = null;
      callback(err, null);
      return;
    }
    callback(null, result());
  }
  return {
    apiKey: 'lftsk_test',
    currentWorkout: function (cb) {
      reply('current', [], cb, function () { return server.current; });
    },
    nextWorkout: function (cb) {
      reply('next', [], cb, function () { return server.next; });
    },
    startWorkout: function (preview, cb) {
      reply('start', [preview], cb, function () {
        server.current = clone(server.next);
        return server.current;
      });
    },
    logSet: function (entryId, setId, completed, cb) {
      reply('set', [entryId, setId, completed], cb, function () {
        server.current.entries.forEach(function (entry) {
          entry.warmupSets.concat(entry.sets).forEach(function (set) {
            if (set.setId === setId) {
              set.completed = completed;
            }
          });
        });
        return server.current;
      });
    },
    finishWorkout: function (startTime, cb) {
      reply('finish', [startTime], cb, function () {
        server.current = null;
        server.next = Object.assign(clone(fixture), { dayName: 'A2' });
        return { nextDay: { dayName: 'A2' } };
      });
    }
  };
};

function setup(options) {
  var server = new FakeLiftosaur();
  var sent = [];
  var store = { deviceId: 'pebble-test' };
  if (!options || options.apiKey !== false) {
    store.apiKey = 'lftsk_test';
  }
  var app = new App({
    storage: {
      getItem: function (k) { return store[k] || null; },
      setItem: function (k, v) { store[k] = v; },
      removeItem: function (k) { delete store[k]; }
    },
    send: function (m) { sent.push(m); },
    sendAll: function (ms) { ms.forEach(function (m) { sent.push(m); }); },
    createClient: function () { return server.client(); },
    setInterval: function () { return 1; },
    clearInterval: function () {}
  });
  return { app: app, server: server, sent: sent, store: store };
}

function types(sent) {
  return sent.map(function (m) { return m.MsgType; });
}

test('without an API key the watch is told to configure one', function () {
  var t = setup({ apiKey: false });
  t.app.onReady();
  assert.strictEqual(t.sent.length, 1);
  assert.strictEqual(t.sent[0].MsgType, MsgType.STATUS);
  assert.match(t.sent[0].Text, /API key/);
});

test('on launch, shows the next workout when none is in progress', function () {
  var t = setup();
  t.app.onReady();
  assert.deepStrictEqual(t.server.calls.map(function (c) { return c.name; }), ['current', 'next']);
  assert.deepStrictEqual(types(t.sent), [MsgType.WORKOUT, MsgType.ENTRY, MsgType.ENTRY, MsgType.DONE]);
  assert.strictEqual(t.sent[0].Started, 0);
});

test('completing a set starts the workout first, then logs it by set id', function () {
  var t = setup();
  t.app.onReady();
  t.sent.length = 0;
  t.app.onAppMessage({ Action: Action.COMPLETE_SET, EntryIndex: 0, SetIndex: 2, Reps: 8, Rpe: 90 });
  var names = t.server.calls.map(function (c) { return c.name; });
  assert.deepStrictEqual(names, ['current', 'next', 'start', 'set']);
  var start = t.server.calls[2].args[0];
  assert.strictEqual(start.programId, 'abc123');
  assert.deepStrictEqual(t.server.calls[3].args, ['squat_barbell', 'asdfgh', { reps: 8, weight: '100kg', rpe: 9 }]);
  assert.strictEqual(t.sent[0].Started, 1);
  assert.strictEqual(t.sent[1].Sets.split(';')[2], '22,5,3,180,8,90,90,100kg');
});

test('undoing a set sends completed: null', function () {
  var t = setup();
  t.server.current = clone(fixture);
  t.app.onReady();
  t.app.onAppMessage({ Action: Action.UNCOMPLETE_SET, EntryIndex: 0, SetIndex: 1, Reps: -1, Rpe: -1 });
  var call = t.server.calls[t.server.calls.length - 1];
  assert.deepStrictEqual(call.args, ['squat_barbell', 'qwertz', null]);
});

test('a failed write reports the error and re-sends the workout to undo the optimistic change', function () {
  var t = setup();
  t.server.current = clone(fixture);
  t.app.onReady();
  t.sent.length = 0;
  t.server.failNext = { status: 400, code: 'missing_set_input', message: 'This set needs more input.' };
  t.app.onAppMessage({ Action: Action.COMPLETE_SET, EntryIndex: 0, SetIndex: 2, Reps: 5, Rpe: -1 });
  assert.strictEqual(t.sent[0].MsgType, MsgType.ERROR);
  assert.deepStrictEqual(types(t.sent.slice(1)), [MsgType.WORKOUT, MsgType.ENTRY, MsgType.ENTRY, MsgType.DONE]);
});

test('a stale set index triggers a refresh instead of writing to the wrong set', function () {
  var t = setup();
  t.server.current = clone(fixture);
  t.app.onReady();
  t.server.calls.length = 0;
  t.app.onAppMessage({ Action: Action.COMPLETE_SET, EntryIndex: 0, SetIndex: 9, Reps: 5, Rpe: -1 });
  assert.deepStrictEqual(t.server.calls.map(function (c) { return c.name; }), ['current']);
});

test('finishing saves the workout and loads the next one', function () {
  var t = setup();
  t.server.current = clone(fixture);
  t.app.onReady();
  t.sent.length = 0;
  t.app.onAppMessage({ Action: Action.FINISH });
  assert.deepStrictEqual(t.server.calls[1].args, [fixture.startTime]);
  assert.strictEqual(t.sent[0].MsgType, MsgType.FINISHED);
  assert.strictEqual(t.sent[0].Text, 'Next up: A2');
  assert.strictEqual(t.sent[1].MsgType, MsgType.WORKOUT);
  assert.strictEqual(t.sent[1].DayName, 'A2');
  assert.strictEqual(t.sent[1].Started, 0);
});

test('polling only re-sends when the workout changed on the phone', function () {
  var t = setup();
  t.server.current = clone(fixture);
  t.app.onReady();
  t.sent.length = 0;
  t.app.poll();
  assert.strictEqual(t.sent.length, 0);
  t.server.current.entries[0].sets[1].completed = { reps: 5, weight: '100kg', rpe: null };
  t.app.poll();
  assert.strictEqual(t.sent[0].MsgType, MsgType.WORKOUT);
});

test('saving settings stores the key and reloads; clearing removes it', function () {
  var t = setup({ apiKey: false });
  t.app.onConfig({ apiKey: 'lftsk_new' });
  assert.strictEqual(t.store.apiKey, 'lftsk_new');
  assert.strictEqual(t.sent[0].MsgType, MsgType.WORKOUT);
  t.app.onConfig({ clearKey: true });
  assert.strictEqual(t.store.apiKey, undefined);
  assert.strictEqual(t.sent[t.sent.length - 1].MsgType, MsgType.STATUS);
});
