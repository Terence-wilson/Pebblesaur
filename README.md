# Pebblesaur

A Pebble watchapp for running [Liftosaur](https://www.liftosaur.com) workouts from your wrist.

- See your next (or in-progress) workout and **switch between exercises**
- **Mark sets as completed** with one click, and undo them
- **Update reps** for any set (AMRAP sets ask for reps automatically)
- Log **RPE** for sets that track it
- A **rest timer** after each set, using the rest time from your program. It vibrates when rest
  is over, even if you've closed the app.
- **Finish the workout**: it's saved to Liftosaur, progressions run, and the next day is loaded

The watch talks to Liftosaur's [REST API](https://www.liftosaur.com/doc/api) ("Running a Workout"
endpoints) through the Pebble phone app. Liftosaur runs the program logic on its server, so
progressions, 1RMs and the next-day pointer update just as if you'd used the Liftosaur app.

## Setup

1. Get a Liftosaur API key: in Liftosaur, go to **Settings → API Keys → Create API Key**. The API
   needs a Liftosaur premium subscription. Keys start with `lftsk_`.
2. Build and install the app (see below).
3. In the Pebble phone app, open Pebblesaur's settings and paste the key.

## Using it

**Workout screen** (opens on launch)

| Row | Select |
| --- | --- |
| Start workout | Starts the workout in Liftosaur (logging a set also starts it) |
| An exercise | Opens it. The row shows sets done and the next set's target |
| Finish workout | Asks to confirm, then saves to Liftosaur |
| Refresh | Reloads from Liftosaur, e.g. after changes on your phone |

**Exercise screen**

| Button | Does |
| --- | --- |
| Up / Down | Move between sets |
| Select on a set to do | Completes it with the target reps and starts the rest timer. AMRAP sets ask for reps first; RPE sets ask for RPE |
| Select on a done set | Change its reps |
| Long-press Select on a set | Options: log with different reps, mark not done, start rest timer |
| Select on **Next exercise** / **Previous exercise** | Switch exercises |

After you complete a set, the selection moves to the next set. When every set is done it moves to
**Next exercise**.

**Rest timer**

| Button | Does |
| --- | --- |
| Up / Down | Add / remove 15 seconds |
| Select | End rest |
| Back | Hide the timer. It keeps counting in the screen headers |

The watch vibrates when rest is over, then counts how long you've gone over. If you leave the app,
a wakeup reopens it when rest ends. Very short rests may not get a wakeup, because Pebble needs
some lead time to schedule one.

**Reps / RPE picker**: Up / Down change the value, Select confirms, Back cancels.

## Building

You need the Pebble SDK (`pip install pebble-tool`, then `pebble sdk install latest`).

```sh
pebble build
pebble install --emulator basalt   # or --phone <ip>
```

Targets: basalt (Pebble Time), chalk (Time Round), diorite (Pebble 2), emery (Time 2) and flint
(Pebble 2 Duo). Aplite isn't supported because it doesn't have enough memory.

## Tests

```sh
npm test           # Phone-side logic (Node 18+)
make -C test/c     # Watch-side workout model, built natively with sanitizers
```

## How it works

```
src/c/            Watch app (C)
  main.c          Startup and wiring
  model.c         The watch's compact copy of the workout
  comm.c          AppMessage protocol with the phone
  rest.c          Rest timer, persisted, with wakeup
  actions.c       Complete / edit / undo a set, finish the workout
  windows/        Workout, exercise, rest, number picker, options, message
src/pkjs/         Phone side (PebbleKit JS)
  app.js          Turns watch actions into Liftosaur calls, keeps the watch in sync
  liftosaur.js    Liftosaur API client
  protocol.js     Workout <-> watch message encoding
  config.js       Settings page (API key)
```

The phone owns the workout. The watch sends actions ("complete set 2 of exercise 1 with 8 reps")
and the phone sends the whole updated workout back. While you're logging a set, the watch shows it
as pending (a dot in the circle) until Liftosaur confirms. If a call fails, the watch shows the
error and goes back to Liftosaur's state. While a workout is in progress and the app is open, the
phone checks Liftosaur every 30 seconds, so sets logged on your phone show up on the watch.

### Limits

- Up to 16 exercises and 12 sets per exercise (warmups included) are shown.
- Weight is logged as programmed. Change weights in the Liftosaur app.
- Timed sets are logged with their programmed hold time.
- Exercises whose program asks for state variables are logged with the current values.
- Adding, removing and reordering exercises or sets happens in the Liftosaur app.
