# PassTheFlag

> This is a fork of [FiringSquad's original work](https://forums.bzflag.org/viewtopic.php?f=79&t=16493). There may or may not be future changes/development to this fork but for the moment it has a hack to fix undefined behavior for team flags after they're captured.

PassTheFlag plugin allows the user to throw the flag they are currently holding. Created by FiringSquad with thanks to mrapple for help with the API.

This current version will not compile under Windows as I access headers other than the "bzfsAPI.h" which contain items that are not exported from the Windows bzfs. There may be ways around this, but for the moment servers on the Windows platform will not be supported for this plugin.


## Plugin command Line:

```
-loadplugin PLUGINNAME[,nopermsneeded][,teamflags|customflags=[V,QT,...]][,passwhiledying|pass2nontkiller|pass2anykiller][,maxwait=<0.0 .. 3.0>][,mwmodify][,dist=<0.0 .. 20.0>][,jboost=<0.0 .. 5.0>][,steps=<0 .. 20>][,debugaccess=<pwd>],
```

The options are available to set things other than the default values.

- `nopermsneeded` - gives everybody permission to adjust config - primarily used for testing - user not notified of setting
- `teamflags` will pass only team flags
- `customflags` lets you specicify specific flags default is all flags. You need to enclose the selection in '[' and ']' characters.
- `passwhiledying` is equivalent to /fpass passondeath=on
- `pass2nontkiller` is equivalent to /fpass passondeath=hurt
- `pass2anykiller` is equivalent to /fpass passondeath=hurt and /fpass hurt=killr
- `mwmodify` will enable "/fpass maxwait" command from modifying the value
- `debugaccess` must be the last entry. If specified the password can be used to allow any client with Admin privileges to get debug messages and modify the mwmodify value (e.g. "...jboost=2.0,debugaccess=1,steps=5,SpyHole" would mean the client would have to enter "/fpass debugaccess=1,steps=5,SpyHole" to get debug access

The options do not have to be added at loadplugin time, but if they are added then they need to added in the order shown above.

```
  Example:   -loadplugin PassTheFlag,teamflags,dist=6.0,steps=5      is fine
  but        -loadplugin PassTheFlag,dist=6.0,teamflags,steps=5      will result in an error.
```

## In-game commands:

FPass commands: [help|stat|off|on|immediate|reset|allflags|teamflags|[customflags|toggleflags]={V,QT,...}|dist=< 0.0 .. 20.0>|steps=<0 .. 20>...|fmsg=[off|all|player]|passondeath=[on|off|hurts]|hurt=[killr|nontker]|jboost=< 0.2 .. 5.0>|debugaccess=<pwd>

```
  /fpass:                      displays command options
  /fpass stat:                 displays the current settings
  /fpass help:                 displays a description of the basic concepts and then displays current settings and command options
  /fpass [on|off|immediate]:   enables/disables flag passing - immediate will forget any fancy stuff and just send the flag flying every time (enabled by default, requires COUNTDOWN permission)
  /fpass reset                 sets everything back to the values set up at loadplugin time (not default values, requires Admin)
  /fpass fmsg=[on|off|player]: turns on/off the "Pass Fumbled" message  (player by default, requires Admin)
  /fpass steps=0 .. 20:        Sets the number of attempts made to find a safe flag-landing.  (5 by default, requires Admin)
  /fpass dist=0.0 .. 20.0:     Sets how far the flag should travel.  (6.0 by default, requires Admin)
  /fpass jboost=0.2 .. 5.0:    Sets how far extra the flag should travel while jumping. Note values below 1.0 reverse the jumping/falling rule  (6.0 by default, requires Admin)
  /fpass [allflags|teamflags]  Specifies that all flags or only team flags may be thrown. (allflags by default, requires Admin)
  /fpass customflags={...}     Allows you to specify a custom set of flags that may be passed (requires Admin)
  /fpass toggleflags={...}     Enables/Disables passing of the flags in the list depending on their current status (requires Admin)
  /fpass passondeath=[on|off|hurts]:  Controls what will happen if the user is killed while holding a flag. Kills by a non-player will result in a normal drop. (off by default, requires Admin)
  /fpass hurt=[killr|nontker]  Sets the definition of "hurts" option. Pass to killer or pass only if not a TK (nontker by default, requires Admin)
  /fpass maxwait=0.0 .. 3.0    Sets the maximum number of seconds I will wait before processing a FlagDropEvent. If I have heard nothing from the client, then I assume that it is a normal pass. Default 0.1 seconds.
  /fpass debugaccess=<pwd>     If password matches the one in loadplugin EXACTLY then debug messages will be sent to the client and the client will have access to maxwait
```

## Notes

- Essentially the flag flies in the direction you are traveling and the faster you go the further it flies.
- You can even send it backwards and through walls.
- The distance the flag flies is configurable and is based on the "dist" value.
- Jumping effects the distance too. If you are rising the flag will go "jboost" times as far if you are falling then it will "1/jboost" as far. Of course, jumping is likely to result in you being
- shot as you land.
- I should probably adjust this to take account of how close to the jump zenith the player is when deciding on how to apply the "jumpboost", as this will give greater control by the player rather
- than the simple cut-off point between rising and falling, but that's for another day.
- If the landing position of the flag is unsafe, then it is considered a fumble.
- Depending on the configuration, the plugin may try to find a safe location closer to you.
- It does this by cutting the distance into "steps" intervals and checking them for a safe landing starting from the furthest one.
- If no safe location is found then a standard drop occurs.

<!-- -->

- It takes a bit of getting used to, but there is a definite skill in getting the flag to fly to where you want and it was this requirement for a level of skill that I was looking for.
- I had considered options like locking on a player and passing to them, but that could result in instant caps etc. and I don't think that would add anything to gameplay.

<!-- -->

- The speed the flag travels at is effected by gravity, `BZDB_FLAGALTITUDE` & `BZDB_SHIELDFLIGHT`

<!-- -->

- I added the "passondeath" option after I saw some frustration when a player kills the flag-carrier but the flag flies away anyway.
- Also some strangeness occurred when a player died because of a Cap or was kicked.
- Now passing will never occur unless the death is at the hands of another player.
- passondeath=off will just result in the flag always just being dropped.
- passondeath=on will send the flag on its way depending on the movement of the player when killed.
- passondeath=hurts will just result in the flag being dropped or passed towards the killer, depending on how "hurt" is configured.
- It is intended that other forms of pain could be added later. e.g. flies away from teambase or longer pause to spawn. Though I will wait to hear from map-makers before adding such options.

<!-- -->

- This passondeath was quite complex to add. The problem is that the client sends the "Drop Flag" message before it send the "I Died" message.
- This meant that I had to delay processing the "Drop Flag" event to allow the client to send me further information.
- If I hear nothing in "maxwait" seconds from this client, then I will assume a pass was wanted. I make this assumption as normally long waits only occur when there is no info to send.
- Being dead is info. :-)
- If allowed on the loadplugin command, this maxwait can be adjusted from the client using the "/fpass maxwait" command.
- This will allow server owners to tweak the value until they are happy that it works well and then they can set the value in stone at loadplugin time.

<!-- -->

- There was a problem where I get a `bz_ePlayerUpdateEvent` generated by another plugin in response to the flag-drop event. I assumed, wrongly, that the event came from the client and so the server would have enough info to make an informed choice. I now ignore any bz_ePlayerUpdateEvent generate less than 1/25 of a second after the flag drop event.

<!-- -->

- The command-line parser of bzfs had problems dealing with plugin parameters in braces so I had to use the square brackets. Unfortunately this causes confusion for the BNF notation in the help, but it couldn't be helped and other suitable characters would cause similar difficulties. I left the braces for the in-games commands since those users might not be as technically minded. While this might cause some confusion, having different formats for command-line and in-game, I think it's an acceptable compromise.

## License

[LGPLv3](https://github.com/allejo/PassTheFlag/blob/master/LICENSE.md)
