class WAAPI_URI:
    # Retrieve global Wwise information.
    ak_wwise_core_getinfo = u"ak.wwise.core.getInfo"
    # Bring Wwise main window to foreground. Refer to SetForegroundWindow and AllowSetForegroundWindow on MSDN for more information on the restrictions. Refer to ak.wwise.core.getInfo to obtain the Wwise process ID for AllowSetForegroundWindow.
    ak_wwise_ui_bringtoforeground = u"ak.wwise.ui.bringToForeground"
    # Asynchronously post an Event to the sound engine (by event ID). See <tt>AK::SoundEngine::PostEvent</tt>.
    ak_soundengine_postevent = u"ak.soundengine.postEvent"
    # Execute an action on all nodes that are referenced in the specified event in an action of type play. See <tt>AK::SoundEngine::ExecuteActionOnEvent</tt>.
    ak_soundengine_executeactiononevent = u"ak.soundengine.executeActionOnEvent"
    # Register a game object. Registering a game object twice does nothing. Unregistering it once unregisters it no matter how many times it has been registered. See <tt>AK::SoundEngine::RegisterGameObj</tt>.
    ak_soundengine_registergameobj = u"ak.soundengine.registerGameObj"
    # Stop the current content, associated to the specified playing ID, from playing. See <tt>AK::SoundEngine::StopPlayingID</tt>.
    ak_soundengine_stopplayingid = u"ak.soundengine.stopPlayingID"
    # Stop playing the current content associated to the specified game object ID. If no game object is specified, all sounds will be stopped. See <tt>AK::SoundEngine::StopAll</tt>.
    ak_soundengine_stopall = u"ak.soundengine.stopAll"
    # Display a message in the profiler.
    ak_soundengine_postmsgmonitor = u"ak.soundengine.postMsgMonitor"
    # Set a game object's obstruction and occlusion levels. This function is used to affect how an object should be heard by a specific listener. See <tt>AK::SoundEngine::SetObjectObstructionAndOcclusion</tt>.
    ak_soundengine_setobjectobstructionandocclusion = u"ak.soundengine.setObjectObstructionAndOcclusion"
    # Set the output bus volume (direct) to be used for the specified game object. See <tt>AK::SoundEngine::SetGameObjectOutputBusVolume</tt>.
    ak_soundengine_setgameobjectoutputbusvolume = u"ak.soundengine.setGameObjectOutputBusVolume"
    # Set the Auxiliary Busses to route the specified game object. See <tt>AK::SoundEngine::SetGameObjectAuxSendValues</tt>.
    ak_soundengine_setgameobjectauxsendvalues = u"ak.soundengine.setGameObjectAuxSendValues"
    # Post the specified Trigger. See <tt>AK::SoundEngine::PostTrigger</tt>.
    ak_soundengine_posttrigger = u"ak.soundengine.postTrigger"
    # Set the State of a Switch Group. See <tt>AK::SoundEngine::SetSwitch</tt>.
    ak_soundengine_setswitch = u"ak.soundengine.setSwitch"
    # Reset the value of a real-time parameter control to its default value, as specified in the Wwise project. See <tt>AK::SoundEngine::ResetRTPCValue</tt>.
    ak_soundengine_resetrtpcvalue = u"ak.soundengine.resetRTPCValue"
    # Set the value of a real-time parameter control. See <tt>AK::SoundEngine::SetRTPCValue</tt>.
    ak_soundengine_setrtpcvalue = u"ak.soundengine.setRTPCValue"
    # Set a listener's spatialization parameters. This lets you define listener-specific volume offsets for each audio channel. See <tt>AK::SoundEngine::SetListenerSpatialization</tt>.
    ak_soundengine_setlistenerspatialization = u"ak.soundengine.setListenerSpatialization"
    # Set multiple positions for a single game object. Setting multiple positions for a single game object is a way to simulate multiple emission sources while using the resources of only one voice. This can be used to simulate wall openings, area sounds, or multiple objects emitting the same sound in the same area. See <tt>AK::SoundEngine::SetMultiplePositions</tt>.
    ak_soundengine_setmultiplepositions = u"ak.soundengine.setMultiplePositions"
    # Set the position of a game object. See <tt>AK::SoundEngine::SetPosition</tt>.
    ak_soundengine_setposition = u"ak.soundengine.setPosition"
    # Set the scaling factor of a game object. Modify the attenuation computations on this game object to simulate sounds with a larger or smaller area of effect. See <tt>AK::SoundEngine::SetScalingFactor</tt>.
    ak_soundengine_setscalingfactor = u"ak.soundengine.setScalingFactor"
    # Set a the default active listeners for all subsequent game objects that are registered. See <tt>AK::SoundEngine::SetDefaultListeners</tt>.
    ak_soundengine_setdefaultlisteners = u"ak.soundengine.setDefaultListeners"
    # Set a single game object's active listeners. By default, all new game objects have no listeners active, but this behavior can be overridden with <tt>SetDefaultListeners()</tt>. Inactive listeners are not computed. See <tt>AK::SoundEngine::SetListeners</tt>.
    ak_soundengine_setlisteners = u"ak.soundengine.setListeners"
    # Seek inside all playing objects that are referenced in Play Actions of the specified Event. See <tt>AK::SoundEngine::SeekOnEvent</tt>.
    ak_soundengine_seekonevent = u"ak.soundengine.seekOnEvent"
    # Unregister a game object. Registering a game object twice does nothing. Unregistering it once unregisters it no matter how many times it has been registered. Unregistering a game object while it is in use is allowed, but the control over the parameters of this game object is lost. For example, say a sound associated with this game object is a 3D moving sound. It will stop moving when the game object is unregistered, and there will be no way to regain control over the game object. See <tt>AK::SoundEngine::UnregisterGameObj</tt>.
    ak_soundengine_unregistergameobj = u"ak.soundengine.unregisterGameObj"
    # Retrieve the list of topics to which a client can subscribe.
    ak_wwise_waapi_gettopics = u"ak.wwise.waapi.getTopics"
    # Retrieve the list of functions.
    ak_wwise_waapi_getfunctions = u"ak.wwise.waapi.getFunctions"
    # Retrieve the JSON schema of a Waapi URI.
    ak_wwise_waapi_getschema = u"ak.wwise.waapi.getSchema"
    # Opens a project, specified by path.
    ak_wwise_ui_project_open = u"ak.wwise.ui.project.open"
    # Closes the current project.
    ak_wwise_ui_project_close = u"ak.wwise.ui.project.close"
    # Saves the current project.
    ak_wwise_core_project_save = u"ak.wwise.core.project.save"
    # Renames an object.
    ak_wwise_core_object_setname = u"ak.wwise.core.object.setName"
    # Sets an object's reference value.
    ak_wwise_core_object_setreference = u"ak.wwise.core.object.setReference"
    # Sets a property value of an object for a specific platform. Refer to \ref wobjects_index for more information on the properties available on each object type.
    ak_wwise_core_object_setproperty = u"ak.wwise.core.object.setProperty"
    # Sets the object's notes.
    ak_wwise_core_object_setnotes = u"ak.wwise.core.object.setNotes"
    # Executes a command. Some commands can take a list of objects as parameter. Refer to \ref globalcommandsids for the available commands.
    ak_wwise_ui_commands_execute = u"ak.wwise.ui.commands.execute"
    # Get the list of commands.
    ak_wwise_ui_commands_getcommands = u"ak.wwise.ui.commands.getCommands"
    # Retrieves the list of objects currently selected by the user in the active view.
    ak_wwise_ui_getselectedobjects = u"ak.wwise.ui.getSelectedObjects"
    # Gets the specified attenuation curve for a given attenuation object.
    ak_wwise_core_object_getattenuationcurve = u"ak.wwise.core.object.getAttenuationCurve"
    # Sets the specified attenuation curve for a given attenuation object.
    ak_wwise_core_object_setattenuationcurve = u"ak.wwise.core.object.setAttenuationCurve"
    # Assign a Switch Container's child to a Switch. This is the equivalent of doing a drag&drop of the child to a state in the Assigned Objects view. The child is always added at the end for each state.
    ak_wwise_core_switchcontainer_addassignment = u"ak.wwise.core.switchContainer.addAssignment"
    # Remove an assignment between a Switch Container's child and a State.
    ak_wwise_core_switchcontainer_removeassignment = u"ak.wwise.core.switchContainer.removeAssignment"
    # Returns the list of assignments between a Switch Container's children and states.
    ak_wwise_core_switchcontainer_getassignments = u"ak.wwise.core.switchContainer.getAssignments"
    # Creates an object of type 'type', as a child of 'parent'.
    ak_wwise_core_object_create = u"ak.wwise.core.object.create"
    # Moves an object to the given parent.
    ak_wwise_core_object_move = u"ak.wwise.core.object.move"
    # Copies an object to the given parent.
    ak_wwise_core_object_copy = u"ak.wwise.core.object.copy"
    # Deletes the specified object.
    ak_wwise_core_object_delete = u"ak.wwise.core.object.delete"
    # Performs a query, returns specified data for each object in query result. Refer to \ref waapi_query for more information.
    ak_wwise_core_object_get = u"ak.wwise.core.object.get"
    # Scripted object creation and audio file import. The contents of this command very closely mirror that of a tab-delimited import file. See \ref ak_wwise_core_audio_importtabdelimited.
    ak_wwise_core_audio_import = u"ak.wwise.core.audio.import"
    # Scripted object creation and audio file import from a tab-delimited file.
    ak_wwise_core_audio_importtabdelimited = u"ak.wwise.core.audio.importTabDelimited"
    # Connects the Wwise Authoring application to a Wwise Sound Engine running executable. The host must be running code with communication enabled.
    ak_wwise_core_remote_connect = u"ak.wwise.core.remote.connect"
    # Disconnects the Wwise Authoring application from a connected Wwise Sound Engine running executable.
    ak_wwise_core_remote_disconnect = u"ak.wwise.core.remote.disconnect"
    # Retrieves all consoles available for connecting Wwise Authoring to a Sound Engine instance.
    ak_wwise_core_remote_getavailableconsoles = u"ak.wwise.core.remote.getAvailableConsoles"
    # Retrieves the connection status.
    ak_wwise_core_remote_getconnectionstatus = u"ak.wwise.core.remote.getConnectionStatus"
    # Begins an undo group. Make sure to call ak.wwise.core.endUndoGroup exactly once for every ak.wwise.core.beginUndoGroup call you make. Calls to ak.wwise.core.beginUndoGroup can be nested.
    ak_wwise_core_undo_begingroup = u"ak.wwise.core.undo.beginGroup"
    # Cancels the last undo group. Please note that this does not revert the operations made since the last ak.wwise.core.beginUndoGroup call.
    ak_wwise_core_undo_cancelgroup = u"ak.wwise.core.undo.cancelGroup"
    # Ends the last undo group.
    ak_wwise_core_undo_endgroup = u"ak.wwise.core.undo.endGroup"
    # Retrieves the list of all object types.
    # \deprecated in favor of ak.wwise.core.object.getTypes
    ak_wwise_core_plugin_getlist = u"ak.wwise.core.plugin.getList"
    # Retrieves the list of all object types.
    ak_wwise_core_object_gettypes = u"ak.wwise.core.object.getTypes"
    # Retrieves information about an object property.
    # \deprecated in favor of ak.wwise.core.object.getPropertyInfo
    ak_wwise_core_plugin_getproperty = u"ak.wwise.core.plugin.getProperty"
    # Retrieves information about an object property.
    ak_wwise_core_object_getpropertyinfo = u"ak.wwise.core.object.getPropertyInfo"
    # Retrieves the list of property names for an object.
    # \deprecated in favor of ak.wwise.core.object.getPropertyNames
    ak_wwise_core_plugin_getproperties = u"ak.wwise.core.plugin.getProperties"
    # Retrieves the list of property names for an object.
    ak_wwise_core_object_getpropertynames = u"ak.wwise.core.object.getPropertyNames"
    # Retrieves the status of a property.
    ak_wwise_core_object_ispropertyenabled = u"ak.wwise.core.object.isPropertyEnabled"
    # Enables debug assertions. Every call to enableAsserts with false increments the ref count. Calling with true will decrement the ref count. This is only available with Debug builds.
    ak_wwise_debug_enableasserts = u"ak.wwise.debug.enableAsserts"
    # Private use only.
    ak_wwise_debug_testassert = u"ak.wwise.debug.testAssert"
    # Retrieves a SoundBank's inclusion list.
    ak_wwise_core_soundbank_getinclusions = u"ak.wwise.core.soundbank.getInclusions"
    # Modifies a SoundBank's inclusion list.  The 'operation' argument determines how the 'inclusions' argument modifies the SoundBank's inclusion list; 'inclusions' may be added to / removed from / replace the SoundBank's inclusion list.
    ak_wwise_core_soundbank_setinclusions = u"ak.wwise.core.soundbank.setInclusions"
    # Creates a transport object for the given Wwise object.  The return transport object can be used to play, stop, pause and resume the Wwise object via the other transport functions.
    ak_wwise_core_transport_create = u"ak.wwise.core.transport.create"
    # Destroys the given transport object.
    ak_wwise_core_transport_destroy = u"ak.wwise.core.transport.destroy"
    # Gets the state of the given transport object.
    ak_wwise_core_transport_getstate = u"ak.wwise.core.transport.getState"
    # Returns the list of transport objects.
    ak_wwise_core_transport_getlist = u"ak.wwise.core.transport.getList"
    # Executes an action on the given transport object, or all transports if no transport is specified.
    ak_wwise_core_transport_executeaction = u"ak.wwise.core.transport.executeAction"
    # Get the min/max peak pairs, in a given region of an audio source, as a collection of binary strings (one per channel). If getCrossChannelPeaks is true, there will be only one binary string representing peaks across all channels globally.
    ak_wwise_core_audiosourcepeaks_getminmaxpeaksinregion = u"ak.wwise.core.audioSourcePeaks.getMinMaxPeaksInRegion"
    # Get the min/max peak pairs in the entire trimmed region of an audio source, for each channel, as an array of binary strings (one per channel). If getCrossChannelPeaks is true, there will be only one binary string representing peaks across all channels globally.
    ak_wwise_core_audiosourcepeaks_getminmaxpeaksintrimmedregion = u"ak.wwise.core.audioSourcePeaks.getMinMaxPeaksInTrimmedRegion"
    # Sent when an object reference is changed.
    ak_wwise_core_object_referencechanged = u"ak.wwise.core.object.referenceChanged"
    # Sent when an assignment is added to a Switch Container.
    ak_wwise_core_switchcontainer_assignmentadded = u"ak.wwise.core.switchContainer.assignmentAdded"
    # Sent when an assignment is removed from a Switch Container.
    ak_wwise_core_switchcontainer_assignmentremoved = u"ak.wwise.core.switchContainer.assignmentRemoved"
    # Sent when an object is renamed.
    ak_wwise_core_object_namechanged = u"ak.wwise.core.object.nameChanged"
    # Sent when the object's notes are changed.
    ak_wwise_core_object_noteschanged = u"ak.wwise.core.object.notesChanged"
    # Sent when an object is created.
    ak_wwise_core_object_created = u"ak.wwise.core.object.created"
    # Sent prior to an object's deletion.
    ak_wwise_core_object_predeleted = u"ak.wwise.core.object.preDeleted"
    # Sent following an object's deletion.
    ak_wwise_core_object_postdeleted = u"ak.wwise.core.object.postDeleted"
    # Sent when an object is added as a child to another object.
    ak_wwise_core_object_childadded = u"ak.wwise.core.object.childAdded"
    # Sent when an object is removed from the children of another object.
    ak_wwise_core_object_childremoved = u"ak.wwise.core.object.childRemoved"
    # Sent when one or many curves are changed.
    ak_wwise_core_object_curvechanged = u"ak.wwise.core.object.curveChanged"
    # Sent when an attenuation curve is changed.
    ak_wwise_core_object_attenuationcurvechanged = u"ak.wwise.core.object.attenuationCurveChanged"
    # Sent when an attenuation curve's link/unlink is changed.
    ak_wwise_core_object_attenuationcurvelinkchanged = u"ak.wwise.core.object.attenuationCurveLinkChanged"
    # Sent when the watched property of an object changes.
    ak_wwise_core_object_propertychanged = u"ak.wwise.core.object.propertyChanged"
    # Sent when the selection changes in the project.
    ak_wwise_ui_selectionchanged = u"ak.wwise.ui.selectionChanged"
    # Sent when the project has been loaded.
    ak_wwise_core_project_loaded = u"ak.wwise.core.project.loaded"
    # Sent when the project begins closing.
    ak_wwise_core_project_preclosed = u"ak.wwise.core.project.preClosed"
    # Sent when the after the project is completely closed.
    ak_wwise_core_project_postclosed = u"ak.wwise.core.project.postClosed"
    # Sent when the transport's state has changed.
    ak_wwise_core_transport_statechanged = u"ak.wwise.core.transport.stateChanged"
    # Sent when an assert has failed.
    ak_wwise_debug_assertfailed = u"ak.wwise.debug.assertFailed"
