namespace ak {
    namespace soundengine {
        // Set multiple positions for a single game object. Setting multiple positions for a single game object is a way to simulate multiple emission sources while using the resources of only one voice. This can be used to simulate wall openings, area sounds, or multiple objects emitting the same sound in the same area. See <tt>AK::SoundEngine::SetMultiplePositions</tt>.
        static const char* setMultiplePositions =  "ak.soundengine.setMultiplePositions"; 
        // Set the scaling factor of a game object. Modify the attenuation computations on this game object to simulate sounds with a larger or smaller area of effect. See <tt>AK::SoundEngine::SetScalingFactor</tt>.
        static const char* setScalingFactor =  "ak.soundengine.setScalingFactor"; 
        // Asynchronously post an Event to the sound engine (by event ID). See <tt>AK::SoundEngine::PostEvent</tt>.
        static const char* postEvent =  "ak.soundengine.postEvent"; 
        // Set the value of a real-time parameter control. See <tt>AK::SoundEngine::SetRTPCValue</tt>.
        static const char* setRTPCValue =  "ak.soundengine.setRTPCValue"; 
        // Set a game object's obstruction and occlusion levels. This function is used to affect how an object should be heard by a specific listener. See <tt>AK::SoundEngine::SetObjectObstructionAndOcclusion</tt>.
        static const char* setObjectObstructionAndOcclusion =  "ak.soundengine.setObjectObstructionAndOcclusion"; 
        // Set a single game object's active listeners. By default, all new game objects have no listeners active, but this behavior can be overridden with <tt>SetDefaultListeners()</tt>. Inactive listeners are not computed. See <tt>AK::SoundEngine::SetListeners</tt>.
        static const char* setListeners =  "ak.soundengine.setListeners"; 
        // Execute an action on all nodes that are referenced in the specified event in an action of type play. See <tt>AK::SoundEngine::ExecuteActionOnEvent</tt>.
        static const char* executeActionOnEvent =  "ak.soundengine.executeActionOnEvent"; 
        // Set a listener's spatialization parameters. This lets you define listener-specific volume offsets for each audio channel. See <tt>AK::SoundEngine::SetListenerSpatialization</tt>.
        static const char* setListenerSpatialization =  "ak.soundengine.setListenerSpatialization"; 
        // Reset the value of a real-time parameter control to its default value, as specified in the Wwise project. See <tt>AK::SoundEngine::ResetRTPCValue</tt>.
        static const char* resetRTPCValue =  "ak.soundengine.resetRTPCValue"; 
        // Unregister a game object. Registering a game object twice does nothing. Unregistering it once unregisters it no matter how many times it has been registered. Unregistering a game object while it is in use is allowed, but the control over the parameters of this game object is lost. For example, say a sound associated with this game object is a 3D moving sound. It will stop moving when the game object is unregistered, and there will be no way to regain control over the game object. See <tt>AK::SoundEngine::UnregisterGameObj</tt>.
        static const char* unregisterGameObj =  "ak.soundengine.unregisterGameObj"; 
        // Stop the current content, associated to the specified playing ID, from playing. See <tt>AK::SoundEngine::StopPlayingID</tt>.
        static const char* stopPlayingID =  "ak.soundengine.stopPlayingID"; 
        // Set the Auxiliary Busses to route the specified game object. See <tt>AK::SoundEngine::SetGameObjectAuxSendValues</tt>.
        static const char* setGameObjectAuxSendValues =  "ak.soundengine.setGameObjectAuxSendValues"; 
        // Seek inside all playing objects that are referenced in Play Actions of the specified Event. See <tt>AK::SoundEngine::SeekOnEvent</tt>.
        static const char* seekOnEvent =  "ak.soundengine.seekOnEvent"; 
        // Register a game object. Registering a game object twice does nothing. Unregistering it once unregisters it no matter how many times it has been registered. See <tt>AK::SoundEngine::RegisterGameObj</tt>.
        static const char* registerGameObj =  "ak.soundengine.registerGameObj"; 
        // Set a the default active listeners for all subsequent game objects that are registered. See <tt>AK::SoundEngine::SetDefaultListeners</tt>.
        static const char* setDefaultListeners =  "ak.soundengine.setDefaultListeners"; 
        // Set the position of a game object. See <tt>AK::SoundEngine::SetPosition</tt>.
        static const char* setPosition =  "ak.soundengine.setPosition"; 
        // Display a message in the profiler.
        static const char* postMsgMonitor =  "ak.soundengine.postMsgMonitor"; 
        // Set the output bus volume (direct) to be used for the specified game object. See <tt>AK::SoundEngine::SetGameObjectOutputBusVolume</tt>.
        static const char* setGameObjectOutputBusVolume =  "ak.soundengine.setGameObjectOutputBusVolume"; 
        // Set the State of a Switch Group. See <tt>AK::SoundEngine::SetSwitch</tt>.
        static const char* setSwitch =  "ak.soundengine.setSwitch"; 
        // Stop playing the current content associated to the specified game object ID. If no game object is specified, all sounds will be stopped. See <tt>AK::SoundEngine::StopAll</tt>.
        static const char* stopAll =  "ak.soundengine.stopAll"; 
        // Post the specified Trigger. See <tt>AK::SoundEngine::PostTrigger</tt>.
        static const char* postTrigger =  "ak.soundengine.postTrigger";
    } 
    namespace wwise {
        namespace debug {
            // Private use only.
            static const char* testAssert =  "ak.wwise.debug.testAssert"; 
            // Sent when an assert has failed.
            static const char* assertFailed =  "ak.wwise.debug.assertFailed"; 
            // Enables debug assertions. Every call to enableAsserts with false increments the ref count. Calling with true will decrement the ref count. This is only available with Debug builds.
            static const char* enableAsserts =  "ak.wwise.debug.enableAsserts";
        } 
        namespace core {
            namespace audioSourcePeaks {
                // Get the min/max peak pairs, in a given region of an audio source, as a collection of binary strings (one per channel). If getCrossChannelPeaks is true, there will be only one binary string representing peaks across all channels globally.
                static const char* getMinMaxPeaksInRegion =  "ak.wwise.core.audioSourcePeaks.getMinMaxPeaksInRegion"; 
                // Get the min/max peak pairs in the entire trimmed region of an audio source, for each channel, as an array of binary strings (one per channel). If getCrossChannelPeaks is true, there will be only one binary string representing peaks across all channels globally.
                static const char* getMinMaxPeaksInTrimmedRegion =  "ak.wwise.core.audioSourcePeaks.getMinMaxPeaksInTrimmedRegion";
            } 
            namespace remote {
                // Retrieves the connection status.
                static const char* getConnectionStatus =  "ak.wwise.core.remote.getConnectionStatus"; 
                // Retrieves all consoles available for connecting Wwise Authoring to a Sound Engine instance.
                static const char* getAvailableConsoles =  "ak.wwise.core.remote.getAvailableConsoles"; 
                // Disconnects the Wwise Authoring application from a connected Wwise Sound Engine running executable.
                static const char* disconnect =  "ak.wwise.core.remote.disconnect"; 
                // Connects the Wwise Authoring application to a Wwise Sound Engine running executable. The host must be running code with communication enabled.
                static const char* connect =  "ak.wwise.core.remote.connect";
            } 
            // Retrieve global Wwise information.
            static const char* getInfo =  "ak.wwise.core.getInfo"; 
            namespace object {
                // Sent when an object reference is changed.
                static const char* referenceChanged =  "ak.wwise.core.object.referenceChanged"; 
                // Moves an object to the given parent.
                static const char* move =  "ak.wwise.core.object.move"; 
                // Sent when an attenuation curve's link/unlink is changed.
                static const char* attenuationCurveLinkChanged =  "ak.wwise.core.object.attenuationCurveLinkChanged"; 
                // Sent when an object is added as a child to another object.
                static const char* childAdded =  "ak.wwise.core.object.childAdded"; 
                // Retrieves the list of all object types.
                static const char* getTypes =  "ak.wwise.core.object.getTypes"; 
                // Sent when the watched property of an object changes.
                static const char* propertyChanged =  "ak.wwise.core.object.propertyChanged"; 
                // Creates an object of type 'type', as a child of 'parent'.
                static const char* create =  "ak.wwise.core.object.create"; 
                // Performs a query, returns specified data for each object in query result. Refer to \ref waapi_query for more information.
                static const char* get =  "ak.wwise.core.object.get"; 
                // Sent prior to an object's deletion.
                static const char* preDeleted =  "ak.wwise.core.object.preDeleted"; 
                // Sent when an object is renamed.
                static const char* nameChanged =  "ak.wwise.core.object.nameChanged"; 
                // Sent following an object's deletion.
                static const char* postDeleted =  "ak.wwise.core.object.postDeleted"; 
                // Sent when the object's notes are changed.
                static const char* notesChanged =  "ak.wwise.core.object.notesChanged"; 
                // Renames an object.
                static const char* setName =  "ak.wwise.core.object.setName"; 
                // Sets the object's notes.
                static const char* setNotes =  "ak.wwise.core.object.setNotes"; 
                // Sets the specified attenuation curve for a given attenuation object.
                static const char* setAttenuationCurve =  "ak.wwise.core.object.setAttenuationCurve"; 
                // Sets a property value of an object for a specific platform. Refer to \ref wobjects_index for more information on the properties available on each object type.
                static const char* setProperty =  "ak.wwise.core.object.setProperty"; 
                // Copies an object to the given parent.
                static const char* copy =  "ak.wwise.core.object.copy"; 
                // Retrieves the status of a property.
                static const char* isPropertyEnabled =  "ak.wwise.core.object.isPropertyEnabled"; 
                // Retrieves information about an object property.
                static const char* getPropertyInfo =  "ak.wwise.core.object.getPropertyInfo"; 
                // Sets an object's reference value.
                static const char* setReference =  "ak.wwise.core.object.setReference"; 
                // Sent when an attenuation curve is changed.
                static const char* attenuationCurveChanged =  "ak.wwise.core.object.attenuationCurveChanged"; 
                // Sent when an object is created.
                static const char* created =  "ak.wwise.core.object.created"; 
                // Sent when an object is removed from the children of another object.
                static const char* childRemoved =  "ak.wwise.core.object.childRemoved"; 
                // Retrieves the list of property names for an object.
                static const char* getPropertyNames =  "ak.wwise.core.object.getPropertyNames"; 
                // Gets the specified attenuation curve for a given attenuation object.
                static const char* getAttenuationCurve =  "ak.wwise.core.object.getAttenuationCurve"; 
                // Sent when one or many curves are changed.
                static const char* curveChanged =  "ak.wwise.core.object.curveChanged"; 
                // Deletes the specified object.
                static const char* delete_ =  "ak.wwise.core.object.delete";
            } 
            namespace undo {
                // Ends the last undo group.
                static const char* endGroup =  "ak.wwise.core.undo.endGroup"; 
                // Cancels the last undo group. Please note that this does not revert the operations made since the last ak.wwise.core.beginUndoGroup call.
                static const char* cancelGroup =  "ak.wwise.core.undo.cancelGroup"; 
                // Begins an undo group. Make sure to call ak.wwise.core.endUndoGroup exactly once for every ak.wwise.core.beginUndoGroup call you make. Calls to ak.wwise.core.beginUndoGroup can be nested.
                static const char* beginGroup =  "ak.wwise.core.undo.beginGroup";
            } 
            namespace project {
                // Sent when the after the project is completely closed.
                static const char* postClosed =  "ak.wwise.core.project.postClosed"; 
                // Sent when the project has been loaded.
                static const char* loaded =  "ak.wwise.core.project.loaded"; 
                // Sent when the project begins closing.
                static const char* preClosed =  "ak.wwise.core.project.preClosed"; 
                // Saves the current project.
                static const char* save =  "ak.wwise.core.project.save";
            } 
            namespace transport {
                // Gets the state of the given transport object.
                static const char* getState =  "ak.wwise.core.transport.getState"; 
                // Sent when the transport's state has changed.
                static const char* stateChanged =  "ak.wwise.core.transport.stateChanged"; 
                // Creates a transport object for the given Wwise object.  The return transport object can be used to play, stop, pause and resume the Wwise object via the other transport functions.
                static const char* create =  "ak.wwise.core.transport.create"; 
                // Returns the list of transport objects.
                static const char* getList =  "ak.wwise.core.transport.getList"; 
                // Destroys the given transport object.
                static const char* destroy =  "ak.wwise.core.transport.destroy"; 
                // Executes an action on the given transport object, or all transports if no transport is specified.
                static const char* executeAction =  "ak.wwise.core.transport.executeAction";
            } 
            namespace soundbank {
                // Retrieves a SoundBank's inclusion list.
                static const char* getInclusions =  "ak.wwise.core.soundbank.getInclusions"; 
                // Modifies a SoundBank's inclusion list.  The 'operation' argument determines how the 'inclusions' argument modifies the SoundBank's inclusion list; 'inclusions' may be added to / removed from / replace the SoundBank's inclusion list.
                static const char* setInclusions =  "ak.wwise.core.soundbank.setInclusions";
            } 
            namespace audio {
                // Scripted object creation and audio file import. The contents of this command very closely mirror that of a tab-delimited import file. See \ref ak_wwise_core_audio_importtabdelimited.
                static const char* import =  "ak.wwise.core.audio.import"; 
                // Scripted object creation and audio file import from a tab-delimited file.
                static const char* importTabDelimited =  "ak.wwise.core.audio.importTabDelimited";
            } 
            namespace switchContainer {
                // Remove an assignment between a Switch Container's child and a State.
                static const char* removeAssignment =  "ak.wwise.core.switchContainer.removeAssignment"; 
                // Returns the list of assignments between a Switch Container's children and states.
                static const char* getAssignments =  "ak.wwise.core.switchContainer.getAssignments"; 
                // Sent when an assignment is removed from a Switch Container.
                static const char* assignmentRemoved =  "ak.wwise.core.switchContainer.assignmentRemoved"; 
                // Assign a Switch Container's child to a Switch. This is the equivalent of doing a drag&drop of the child to a state in the Assigned Objects view. The child is always added at the end for each state.
                static const char* addAssignment =  "ak.wwise.core.switchContainer.addAssignment"; 
                // Sent when an assignment is added to a Switch Container.
                static const char* assignmentAdded =  "ak.wwise.core.switchContainer.assignmentAdded";
            } 
            namespace plugin {
                // Retrieves the list of all object types.
                // \deprecated in favor of ak.wwise.core.object.getTypes
                static const char* getList =  "ak.wwise.core.plugin.getList"; 
                // Retrieves information about an object property.
                // \deprecated in favor of ak.wwise.core.object.getPropertyInfo
                static const char* getProperty =  "ak.wwise.core.plugin.getProperty"; 
                // Retrieves the list of property names for an object.
                // \deprecated in favor of ak.wwise.core.object.getPropertyNames
                static const char* getProperties =  "ak.wwise.core.plugin.getProperties";
            }
        } 
        namespace ui {
            namespace project {
                // Closes the current project.
                static const char* close =  "ak.wwise.ui.project.close"; 
                // Opens a project, specified by path.
                static const char* open =  "ak.wwise.ui.project.open";
            } 
            // Bring Wwise main window to foreground. Refer to SetForegroundWindow and AllowSetForegroundWindow on MSDN for more information on the restrictions. Refer to ak.wwise.core.getInfo to obtain the Wwise process ID for AllowSetForegroundWindow.
            static const char* bringToForeground =  "ak.wwise.ui.bringToForeground"; 
            namespace commands {
                // Executes a command. Some commands can take a list of objects as parameter. Refer to \ref globalcommandsids for the available commands.
                static const char* execute =  "ak.wwise.ui.commands.execute"; 
                // Get the list of commands.
                static const char* getCommands =  "ak.wwise.ui.commands.getCommands";
            } 
            // Retrieves the list of objects currently selected by the user in the active view.
            static const char* getSelectedObjects =  "ak.wwise.ui.getSelectedObjects"; 
            // Sent when the selection changes in the project.
            static const char* selectionChanged =  "ak.wwise.ui.selectionChanged";
        } 
        namespace waapi {
            // Retrieve the list of topics to which a client can subscribe.
            static const char* getTopics =  "ak.wwise.waapi.getTopics"; 
            // Retrieve the list of functions.
            static const char* getFunctions =  "ak.wwise.waapi.getFunctions"; 
            // Retrieve the JSON schema of a Waapi URI.
            static const char* getSchema =  "ak.wwise.waapi.getSchema";
        }
    }
}
