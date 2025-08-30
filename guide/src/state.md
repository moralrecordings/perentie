# State

State can be described as the set of facts required to recreate the engine at a particular point in time. Perentie tries to adhere to the model-view-controller pattern; there is a set of data (model) which can be changed by the user clicking and triggering code in event callbacks (controller) and displayed as stuff happening on the screen (view). Any information kept outside of the model is considered ephemeral, and must be set up on loading.

Right now, the model data in a Perentie state consists of the following:

- Everything in the variable store
- For all PTActors:
    - Position
    - Facing
    - Room location
- For all PTRooms:
    - Camera position
    - Camera actor
- Current room


## The variable store

The variable store is accessible at any time by calling `PTVars`. We recommend using it to store the bare amount of information required to represent the player's progress. 

The variable store can only be used for storing primitive types; booleans, numbers, strings, and tables. 

You **cannot** store PT objects in the variable store; they will clash with objects created when the engine has started, and some are just pointers to data in memory (e.g. PTImage) which can't be serialised. Perentie will raise this in the log whenever it notices this happening.

## Saving the state

You can save the current state using `PTSaveState(slot)`, where slot is a number from 0-999. Saved states will be stored in your application's local app data directory with the filename `SAVE.000`, `SAVE.001`... all the way to `SAVE.999`. We recommend using slot 0 for autosave/hot reload. Information about which state slots are in use can be fetched using `PTGetSaveStateSummary`.

## Loading the state

When loading a state, Perentie does the following:
- **Send a "reset" event.** This includes the path to the state file to load.
- **Wait until the start of the next frame.** The event loop has finished processing, the scene graph has been rendered, and the display page has been flipped.
- **Close the main Lua thread.** This will destroy and garbage collect all the objects, including threads and references to data outside of Lua.
- **Reset any palette modifications.**
- **Create a new main Lua thread.**
- **Run through the normal init process.** Essentially this imports all of the engine + game Lua code.
- **Unlike a normal init, do not raise a "start" event.**
- **Read the data from the state file.**
- **If the game has set up an event callback with `PTOnLoadState`, call this function with the state data.** If you need to make fixups to the state data so that older save files work on newer versions of the game, here's the place to do it.
- **Apply the state data to the engine.** This will overwrite the contents of the variable store, along with anything mentioned as part of the model.
- **If the game has set up an event callback for the destination room with `PTOnRoomLoad`, call this function.**
