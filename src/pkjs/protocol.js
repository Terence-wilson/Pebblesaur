// Translates between Liftosaur workouts and the compact messages the watch
// understands. Keep in sync with src/c/model.h and src/c/comm.h.
//
// Written in ES5: PebbleKit JS runs on older phone JavaScript engines.

var Action = {
  REFRESH: 1,
  START: 2,
  COMPLETE_SET: 3,
  UNCOMPLETE_SET: 4,
  FINISH: 5
};

var MsgType = {
  STATUS: 1,
  WORKOUT: 2,
  ENTRY: 3,
  DONE: 4,
  ERROR: 5,
  FINISHED: 6
};

var SetFlag = {
  WARMUP: 1 << 0,
  AMRAP: 1 << 1,
  LOG_RPE: 1 << 2,
  ASK_WEIGHT: 1 << 3,
  COMPLETED: 1 << 4,
  UNILATERAL: 1 << 5,
  TIMED: 1 << 6
};

// Watch buffer sizes in bytes, including the terminating NUL.
var Limits = {
  ENTRIES: 16,
  SETS: 12,
  NAME: 32,
  INFO: 32,
  TITLE: 40,
  WEIGHT: 12,
  TEXT: 96
};

var NO_VALUE = -1;

// Cuts a string to fit maxBytes of UTF-8 (minus the NUL) without splitting a
// character.
function truncateUtf8(text, maxBytes) {
  var str = text == null ? '' : String(text);
  var limit = maxBytes - 1;
  var bytes = 0;
  var out = '';
  for (var i = 0; i < str.length; i++) {
    var code = str.charCodeAt(i);
    var ch = str.charAt(i);
    var size;
    if (code >= 0xd800 && code <= 0xdbff && i + 1 < str.length) {
      ch += str.charAt(i + 1);
      size = 4;
    } else if (code < 0x80) {
      size = 1;
    } else if (code < 0x800) {
      size = 2;
    } else {
      size = 3;
    }
    if (bytes + size > limit) {
      break;
    }
    bytes += size;
    out += ch;
    i += ch.length - 1;
  }
  return out;
}

function isNumber(value) {
  return typeof value === 'number' && isFinite(value);
}

// Weights come as "100kg" strings, but tolerate {value, unit} objects and
// bare numbers too.
function formatWeight(weight) {
  if (weight == null) {
    return '';
  }
  if (typeof weight === 'string') {
    return weight.replace(/\s+/g, '');
  }
  if (isNumber(weight)) {
    return String(weight);
  }
  if (typeof weight === 'object' && weight.value != null) {
    return String(weight.value) + (weight.unit || '');
  }
  return '';
}

function intOr(value, fallback) {
  return isNumber(value) ? Math.round(value) : fallback;
}

function rpe10(value) {
  return isNumber(value) ? Math.round(value * 10) : NO_VALUE;
}

// Warmup sets first, then working sets: the order the watch shows them in.
function entrySets(entry) {
  var warmups = (entry.warmupSets || []).map(function (set) {
    return { set: set, warmup: true };
  });
  var working = (entry.sets || []).map(function (set) {
    return { set: set, warmup: !!set.isWarmup };
  });
  return warmups.concat(working);
}

function setFlags(item) {
  var set = item.set;
  var flags = 0;
  if (item.warmup) flags |= SetFlag.WARMUP;
  if (set.isAmrap) flags |= SetFlag.AMRAP;
  if (set.logRpe) flags |= SetFlag.LOG_RPE;
  if (set.askWeight) flags |= SetFlag.ASK_WEIGHT;
  if (set.completed) flags |= SetFlag.COMPLETED;
  if (set.isUnilateral) flags |= SetFlag.UNILATERAL;
  if (set.setTimer != null) flags |= SetFlag.TIMED;
  return flags;
}

// "flags,reps,minReps,timer,doneReps,rpe10,doneRpe10,weight"
function encodeSet(item) {
  var set = item.set;
  var completed = set.completed || null;
  var weight = formatWeight(completed && completed.weight != null ? completed.weight : set.weight);
  var fields = [
    setFlags(item),
    intOr(set.reps, NO_VALUE),
    intOr(set.minReps, NO_VALUE),
    intOr(set.timer, 0),
    completed ? intOr(completed.reps, NO_VALUE) : NO_VALUE,
    rpe10(set.rpe),
    completed ? rpe10(completed.rpe) : NO_VALUE
  ];
  // ';' separates sets, so it can't appear in the free-text weight.
  return fields.join(',') + ',' + truncateUtf8(weight.replace(/;/g, ''), Limits.WEIGHT);
}

function entryInfo(entry) {
  return truncateUtf8(entry.description || entry.notes || '', Limits.INFO);
}

// Returns the messages to send, in order: WORKOUT, one ENTRY per exercise,
// then DONE.
function encodeWorkout(workout, started) {
  var entries = (workout.entries || []).slice(0, Limits.ENTRIES);
  var messages = [{
    MsgType: MsgType.WORKOUT,
    Started: started ? 1 : 0,
    Count: entries.length,
    ProgramName: truncateUtf8(workout.programName, Limits.TITLE),
    DayName: truncateUtf8(workout.dayName, Limits.TITLE)
  }];
  entries.forEach(function (entry, index) {
    messages.push({
      MsgType: MsgType.ENTRY,
      EntryIndex: index,
      Name: truncateUtf8(entry.name || entry.exerciseId, Limits.NAME),
      Info: entryInfo(entry),
      Sets: entrySets(entry).slice(0, Limits.SETS).map(encodeSet).join(';')
    });
  });
  messages.push({ MsgType: MsgType.DONE });
  return messages;
}

// Finds the Liftosaur ids behind the watch's (entry, set) indices.
function locateSet(workout, entryIndex, setIndex) {
  var entry = workout && workout.entries && workout.entries[entryIndex];
  if (!entry) {
    return null;
  }
  var item = entrySets(entry)[setIndex];
  if (!item) {
    return null;
  }
  return { entry: entry, set: item.set };
}

// Builds the `completed` payload for POST /workout/set. Throws an Error with
// a watch-friendly message when the set needs something we can't supply.
function buildCompleted(entry, set, reps, rpe) {
  var completed = { reps: reps };
  if (set.isUnilateral) {
    completed.repsLeft = reps;
  }
  if (set.weight != null) {
    completed.weight = set.weight;
  } else if (set.askWeight) {
    throw new Error('This set needs a weight. Log it in the Liftosaur app.');
  }
  if (isNumber(rpe) && rpe >= 0) {
    completed.rpe = rpe / 10;
  } else if (set.logRpe) {
    if (!isNumber(set.rpe)) {
      throw new Error('This set needs an RPE.');
    }
    completed.rpe = set.rpe;
  }
  // Timed sets: report the programmed hold.
  if (set.setTimer != null) {
    completed.setTimer = set.setTimer;
    if (set.isUnilateral) {
      completed.setTimerLeft = set.setTimer;
    }
  }
  // The watch can't ask for program variables, so confirm the current values.
  var prompted = entry.promptedVars || [];
  if (prompted.length > 0) {
    completed.userVars = {};
    prompted.forEach(function (v) {
      completed.userVars[v.name] = v.value;
    });
  }
  return completed;
}

module.exports = {
  Action: Action,
  MsgType: MsgType,
  SetFlag: SetFlag,
  Limits: Limits,
  NO_VALUE: NO_VALUE,
  truncateUtf8: truncateUtf8,
  formatWeight: formatWeight,
  encodeWorkout: encodeWorkout,
  locateSet: locateSet,
  buildCompleted: buildCompleted
};
