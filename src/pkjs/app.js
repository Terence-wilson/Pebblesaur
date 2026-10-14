// The phone half of Pebblesaur: turns watch actions into Liftosaur API calls
// and keeps the watch's copy of the workout up to date.

var protocol = require('./protocol');

var Action = protocol.Action;
var MsgType = protocol.MsgType;

var POLL_INTERVAL_MS = 30000;
var STORAGE_API_KEY = 'apiKey';
var STORAGE_DEVICE_ID = 'deviceId';

var NO_KEY_MESSAGE = 'Add your Liftosaur API key in the Pebblesaur settings on your phone.';

function randomId() {
  var id = '';
  for (var i = 0; i < 4; i++) {
    id += Math.floor((1 + Math.random()) * 0x10000).toString(16).substring(1);
  }
  return 'pebble-' + id;
}

// deps:
//   storage: { getItem, setItem, removeItem }
//   send(message), sendAll(messages): deliver AppMessages to the watch
//   createClient({ apiKey, deviceId }): a liftosaur.Client
//   setInterval, clearInterval
function App(deps) {
  this.deps = deps;
  this.client = null;
  this.workout = null;
  this.started = false;
  this.lastSentKey = null;
  this.queue = [];
  this.running = false;
  // Bumped by every write so a poll that raced a write is thrown away.
  this.writeSeq = 0;
  this.pollTimer = null;
}

App.prototype.deviceId = function () {
  var id = this.deps.storage.getItem(STORAGE_DEVICE_ID);
  if (!id) {
    id = randomId();
    this.deps.storage.setItem(STORAGE_DEVICE_ID, id);
  }
  return id;
};

App.prototype.ensureClient = function () {
  var apiKey = this.deps.storage.getItem(STORAGE_API_KEY);
  if (!apiKey) {
    this.client = null;
    return null;
  }
  if (!this.client || this.client.apiKey !== apiKey) {
    this.client = this.deps.createClient({ apiKey: apiKey, deviceId: this.deviceId() });
  }
  return this.client;
};

App.prototype.hasApiKey = function () {
  return !!this.deps.storage.getItem(STORAGE_API_KEY);
};

// ---- Talking to the watch ----

App.prototype.sendStatus = function (text) {
  this.workout = null;
  this.started = false;
  this.lastSentKey = null;
  this.stopPolling();
  this.deps.send({ MsgType: MsgType.STATUS, Text: protocol.truncateUtf8(text, protocol.Limits.TEXT) });
};

App.prototype.sendNotice = function (type, text) {
  this.deps.send({ MsgType: type, Text: protocol.truncateUtf8(text, protocol.Limits.TEXT) });
};

// force: the watch is waiting for an answer, so send even if nothing changed.
App.prototype.adopt = function (workout, started, force) {
  this.workout = workout;
  this.started = started;
  var messages = protocol.encodeWorkout(workout, started);
  var key = JSON.stringify(messages);
  if (force || key !== this.lastSentKey) {
    this.lastSentKey = key;
    this.deps.sendAll(messages);
  }
  if (started) {
    this.startPolling();
  } else {
    this.stopPolling();
  }
};

// After a failed write: report it, then re-send the workout so sets the watch
// marked optimistically go back to how they really are.
App.prototype.fail = function (err) {
  var message = (err && err.message) || 'Something went wrong.';
  if (this.workout) {
    this.sendNotice(MsgType.ERROR, message);
    this.adopt(this.workout, this.started, true);
  } else {
    this.sendStatus(message);
  }
  if (err && (err.code === 'set_not_found' || err.code === 'entry_not_found' ||
      err.code === 'no_active_workout' || err.code === 'workout_already_active')) {
    this.enqueue(this.refresh.bind(this));
  }
};

// ---- Action queue: one Liftosaur call chain at a time, in order ----

App.prototype.enqueue = function (task) {
  this.queue.push(task);
  this.runNext();
};

App.prototype.runNext = function () {
  if (this.running || this.queue.length === 0) {
    return;
  }
  this.running = true;
  var self = this;
  var task = this.queue.shift();
  task(function () {
    self.running = false;
    self.runNext();
  });
};

// ---- Liftosaur calls ----

App.prototype.refresh = function (done) {
  var self = this;
  var client = this.ensureClient();
  if (!client) {
    this.sendStatus(NO_KEY_MESSAGE);
    done();
    return;
  }
  client.currentWorkout(function (err, current) {
    if (err) {
      self.sendStatus(err.message);
      done();
      return;
    }
    if (current) {
      self.adopt(current, true, true);
      done();
      return;
    }
    client.nextWorkout(function (err2, next) {
      if (err2) {
        self.sendStatus(err2.code === 'day_not_found' || err2.status === 404
          ? 'No workout found. Pick a program in Liftosaur.'
          : err2.message);
      } else if (next) {
        self.adopt(next, false, true);
      } else {
        self.sendStatus('No workout found. Pick a program in Liftosaur.');
      }
      done();
    });
  });
};

App.prototype.ensureStarted = function (callback) {
  if (this.started) {
    callback(null);
    return;
  }
  var self = this;
  var client = this.ensureClient();
  if (!client) {
    callback(new Error(NO_KEY_MESSAGE));
    return;
  }
  this.writeSeq++;
  client.startWorkout(this.workout, function (err, workout) {
    if (err) {
      callback(err);
      return;
    }
    // Set ids may be minted at start, so look sets up in the started workout.
    self.workout = workout;
    self.started = true;
    callback(null);
  });
};

App.prototype.start = function (done) {
  var self = this;
  if (!this.workout) {
    this.refresh(done);
    return;
  }
  this.ensureStarted(function (err) {
    if (err) {
      self.fail(err);
    } else {
      self.adopt(self.workout, true, true);
    }
    done();
  });
};

// completed: true to log (with reps/rpe10 from the watch), false to undo.
App.prototype.logSet = function (entryIndex, setIndex, completed, reps, rpe10, done) {
  var self = this;
  if (!this.workout) {
    this.fail(new Error('No workout loaded.'));
    done();
    return;
  }
  this.ensureStarted(function (err) {
    if (err) {
      self.fail(err);
      done();
      return;
    }
    var found = protocol.locateSet(self.workout, entryIndex, setIndex);
    if (!found) {
      self.fail({ message: 'The workout changed on your phone. Refreshing.', code: 'set_not_found' });
      done();
      return;
    }
    var payload = null;
    if (completed) {
      try {
        payload = protocol.buildCompleted(found.entry, found.set, reps, rpe10);
      } catch (e) {
        self.fail(e);
        done();
        return;
      }
    }
    self.writeSeq++;
    self.ensureClient().logSet(found.entry.entryId, found.set.setId, payload, function (err2, workout) {
      if (err2) {
        self.fail(err2);
      } else {
        self.adopt(workout, true, true);
      }
      done();
    });
  });
};

App.prototype.finish = function (done) {
  var self = this;
  var client = this.ensureClient();
  if (!client || !this.workout || !this.started) {
    this.fail(new Error('No workout in progress.'));
    done();
    return;
  }
  this.writeSeq++;
  client.finishWorkout(this.workout.startTime, function (err, summary) {
    if (err) {
      self.fail(err);
      done();
      return;
    }
    var next = summary && summary.nextDay && summary.nextDay.dayName;
    self.stopPolling();
    self.workout = null;
    self.started = false;
    self.sendNotice(MsgType.FINISHED, next ? 'Next up: ' + next : 'Saved to Liftosaur.');
    // Show the next workout.
    self.refresh(done);
  });
};

// Picks up sets logged on the phone while the watch app is open.
App.prototype.poll = function () {
  if (!this.started || this.running || this.queue.length > 0) {
    return;
  }
  var self = this;
  var client = this.ensureClient();
  if (!client) {
    return;
  }
  var seq = this.writeSeq;
  this.enqueue(function (done) {
    client.currentWorkout(function (err, current) {
      if (seq === self.writeSeq && !err) {
        if (current) {
          self.adopt(current, true, false);
        } else {
          // Finished or discarded on the phone.
          self.workout = null;
          self.started = false;
          self.refresh(done);
          return;
        }
      }
      done();
    });
  });
};

App.prototype.startPolling = function () {
  if (!this.pollTimer) {
    this.pollTimer = this.deps.setInterval(this.poll.bind(this), POLL_INTERVAL_MS);
  }
};

App.prototype.stopPolling = function () {
  if (this.pollTimer) {
    this.deps.clearInterval(this.pollTimer);
    this.pollTimer = null;
  }
};

// ---- Entry points ----

App.prototype.onReady = function () {
  this.enqueue(this.refresh.bind(this));
};

App.prototype.onAppMessage = function (payload) {
  var self = this;
  var entryIndex = payload.EntryIndex;
  var setIndex = payload.SetIndex;
  switch (payload.Action) {
    case Action.REFRESH:
      this.enqueue(this.refresh.bind(this));
      break;
    case Action.START:
      this.enqueue(this.start.bind(this));
      break;
    case Action.COMPLETE_SET:
      this.enqueue(function (done) {
        self.logSet(entryIndex, setIndex, true, payload.Reps, payload.Rpe, done);
      });
      break;
    case Action.UNCOMPLETE_SET:
      this.enqueue(function (done) {
        self.logSet(entryIndex, setIndex, false, null, null, done);
      });
      break;
    case Action.FINISH:
      this.enqueue(this.finish.bind(this));
      break;
    default:
      console.log('Unknown action: ' + JSON.stringify(payload));
  }
};

// settings: what the config page returned.
App.prototype.onConfig = function (settings) {
  if (settings.clearKey) {
    this.deps.storage.removeItem(STORAGE_API_KEY);
  } else if (settings.apiKey) {
    this.deps.storage.setItem(STORAGE_API_KEY, settings.apiKey);
  }
  this.enqueue(this.refresh.bind(this));
};

module.exports = {
  App: App,
  NO_KEY_MESSAGE: NO_KEY_MESSAGE,
  POLL_INTERVAL_MS: POLL_INTERVAL_MS
};
