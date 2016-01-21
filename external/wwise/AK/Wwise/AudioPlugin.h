//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file
/// Wwise audio plug-in interface, used to implement the Wwise side of a source or effect plug-in.

#ifndef _AK_WWISE_AUDIOPLUGIN_H
#define _AK_WWISE_AUDIOPLUGIN_H

#include "Undo.h"

#include <AK/Wwise/Utilities.h>
#include <AK/SoundEngine/Common/AkSoundEngine.h> /// Dummy assert hook definition.
#include <AK/Wwise/PlatformID.h>

// Audiokinetic namespace
namespace AK
{
	class IXmlTextReader;
	class IXmlTextWriter;

	// Audiokinetic Wwise namespace
	namespace Wwise
	{
		class IPluginMediaConverter
		{
		public:
			/// If the conversion failed the function is responsible of deleting 
			/// any files that may have been created, even the destination file 
			/// in case of error. If the function return false we will use the 
			/// string put in io_pError to display an error message.
			virtual ConversionResult ConvertFile( 
				const GUID & in_guidPlatform,					///< The unique ID of the custom platform being converted for.
				const BasePlatformID & in_basePlatform,			///< The unique ID of the base platform being converted for.
				LPCWSTR in_szSourceFile,						///< Source File to convert data from.
				LPCWSTR in_szDestFile,							///< DestinationFile, must be created by the plug-in.
				AkUInt32 in_uSampleRate,						///< The target sample rate for the converted file, passing 0 will default to the platform default
				AkUInt32 in_uBlockLength,						///< The block length, passing 0 will default to the platform default
				AK::Wwise::IProgress* in_pProgress,				///< Optional Progress Bar controller.
				IWriteString* io_pError							///< Optional error string that can be displayed if ConversionResult is not successful
				) = 0;

			virtual ULONG GetCurrentConversionSettingsHash(
				const GUID & in_guidPlatform,			///< The unique ID of the platform being converted for.
				AkUInt32 in_uSampleRate = 0,			///< The target sample rate for the converted file, passing 0 will default to the platform default.
				AkUInt32 in_uBlockLength = 0			///< The block length, passing 0 will default to the platform default.
				) = 0;
		};

		/// Plug-in property set interface. An instance of this class is created and
		/// assigned to each plug-in, which in turn can use it to manage its properties.
		/// Whenever a property name is specified, it corresponds to the property
		/// name set in the plug-in's XML definition file.
		/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
		/// \sa
		/// - \ref wwiseplugin_xml_properties_tag
		/// - AK::Wwise::IAudioPlugin::SetPluginPropertySet()
		/// - \ref wwiseplugin_propertyset
		class IPluginPropertySet
		{
		public:
			/// Get the value of a property for a specified platform.
			/// \return True if successful, False otherwise
			/// \sa
			/// - \ref wwiseplugin_bank
			virtual bool GetValue( 
				const GUID & in_guidPlatform,	///< The unique ID of the queried platform
				LPCWSTR in_pszPropertyName,		///< The name of the property
				VARIANT & out_varProperty		///< The returned value of the property
				) = 0;
			
			/// Set the value of a property for a specified platform.
			/// \return True if successful, False otherwise.
			virtual bool SetValue( 
				const GUID & in_guidPlatform,	///< The unique ID of the platform to modify
				LPCWSTR in_pszPropertyName,		///< The name of the property
				const VARIANT & in_varProperty	///< The value to set
				) = 0;

			/// Get the RTPC binding status for a specified property.
			/// \return True if property is bound to a RTPC, False otherwise.
			virtual bool PropertyHasRTPC(
				LPCWSTR in_pszPropertyName		///< The name of the property
				) = 0;

			/// This function is called by Wwise to get the current platform's identifier. 
			/// This can be passed to any function that has a parameter
			/// for a platform ID, such as GetValue() or SetValue(), when you want to make
			/// the call for the currently active platform.
			/// \return The unique ID of the current platform
			virtual GUID GetCurrentPlatform() = 0;

			/// This function is called by Wwise to get the current base platform
			/// \return The unique ID of the current base platform
			virtual BasePlatformID GetCurrentBasePlatform() = 0;

			/// This function is called To retrieve the base platforms of the authoring tool.
			virtual BasePlatformID GetDefaultNativeAuthoringPlaybackPlatform() = 0;

			/// This function is called To retrieve the custom platform being used to run while in authoring
			virtual GUID GetAuthoringPlaybackPlatform() = 0;

			/// Use this function to tell Wwise that something other than properties 
			/// has changed within the plugin.  This will set the plugin dirty (for save)
			/// and GetPluginData will be called when the plugin is about to play in Wwise, to
			/// transfer the internal data to the Sound Engine part of the plugin.
			/// Use ALL_PLUGIN_DATA_ID to tell that all the data has to be refreshed.
			virtual void NotifyInternalDataChanged(AkPluginParamID in_idData) = 0;

			/// Call this function when you are about to log an undo event to know if Wwise is 
			/// in a state where undos are enabled.  Undo logging can be disabled for a particular
			/// plugin object if it already lives in the undo stack or in the clipboard.
			virtual bool CanLogUndos() = 0;

			/// Obtain the Undo Manager.  The Undo Manager can be used to group undo together or
			/// to check the status of the undo system.
			virtual AK::Wwise::IUndoManager * GetUndoManager() = 0;

			/// Obtain licensing status for a plug-in-specific asset ID.
			virtual void GetAssetLicenseStatus( 
				const GUID & in_guidPlatform,			///< GUID of the platform
				AkUInt32 in_uAssetID,					///< ID of the asset
				AK::Wwise::LicenseType & out_eType,		///< License Type
				AK::Wwise::LicenseStatus & out_eStatus, ///< License Status
				UINT32 & out_uDaysToExpiry				///< Days until license expiry
				) = 0;
		};

		/// Plug-in object store interface. An instance of this class is created and
		/// assigned to each plug-in, which in turn can use it to manage its inner objects.
		/// Inner objects can be created from the inner types defined in the plug-in's XML 
		/// definition file.
		/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
		/// \sa
		/// - AK::Wwise::IAudioPlugin::SetPluginObjectStore()
		/// - \ref wwiseplugin_objectstore
		/// - \ref wwiseplugin_xml_properties_tag
		class IPluginObjectStore
		{
		public:
			/// Inserts an object into the specified list at the specified position.  To create objects,
			/// use CreateObject.  Note that an object can only be inside one list.
			/// Pass (unsigned int)-1 as the index to insert at the end of the list
			virtual void InsertObject( 
				LPCWSTR in_pszListName,
				unsigned int in_uiIndex,
				IPluginPropertySet* in_pPropertySet
				) = 0;
			
			/// Removes an object from its list.  The list is not specified and is automatically found.
			/// The function \c DeleteObject must be called if the object is no longer necessary.
			/// \return True if successful, False otherwise
			virtual bool RemoveObject( 
				IPluginPropertySet* in_pPropertySet
				) = 0;

			/// Gets an object inside the specified list at the specified position.
			/// \return The object in the specified list at the specified position, NULL if list or index are invalid
			virtual IPluginPropertySet* GetObject( 
				LPCWSTR in_pszListName,
				unsigned int in_uiIndex
				) const = 0;

			/// Get the number of object inside the specified list.
			/// \return Number of object inside the specified list.
			virtual unsigned int GetObjectCount( 
				LPCWSTR in_pszListName
				) const = 0;

			/// Create a new object instance of the specified type.  The type must be defined in the Plugin XML definition.
			/// See the \c InnerTypes section in the plug-in definition.
			/// \return The instance of the newly created object, NULL if not successful
			virtual IPluginPropertySet* CreateObject( 
				LPCWSTR in_pszType
				) = 0;

			/// Frees the object.  It will also remove the object from its list if the object is still in a list.
			/// Do not use the object after calling this function.
			virtual void DeleteObject( 
				IPluginPropertySet* in_pPropertySet
				) = 0;

			/// Gets the number of lists.
			/// \return The number of lists.
			virtual unsigned int GetListCount() const = 0;

			/// Get the name of the list at the specified position.  The buffer must be large enough to copy the list name.
			/// When the buffer is too small, the function do not write to the buffer and return zero.
			/// \return Number of characters written to the buffer, zero if failed.
			virtual unsigned int GetListName( 
				unsigned int in_uiListIndex,
				LPWSTR out_pszListName,
				unsigned int in_uiBufferSize
				) const = 0;
		};

		/// Plug-in object media interface. An instance of this class is created and
		/// assigned to each plug-in that supports media file handling.
		/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
		/// \sa
		/// - AK::Wwise::IAudioPlugin::SetPluginObjectMedia()
		class IPluginObjectMedia
		{
		public:
			
			/// Requests to set the specified file as a data input file.
			virtual bool SetMediaSource( 
				LPCWSTR in_pszFilePathToImport,	///< File path
				unsigned int in_Index = 0,		///< Optional index
				bool in_bReplace = false		///< Optional: set to true to replace existing file if the name is already in used
				) = 0;

			/// Requests to remove the specified index file s a data input file.
			virtual void RemoveMediaSource( 
				unsigned int in_Index = 0 	///< Optional index
				) = 0;

			/// Retrieve the number of dataSource, it will be then possible to
			/// call GetMediaFileName or RemoveMediaSource using the provided index
			virtual unsigned int GetMediaSourceCount() const = 0;

			/// Retrieve the file name of the source plug-in data relative to the 
			/// original directory at the specified index.
			/// Mostly used to allow the Plug-in to display this information.
			/// \return Number of characters written to the buffer, zero if failed.
			virtual unsigned int GetMediaSourceFileName(
				LPWSTR out_pszFileName,			///< Relative path of the associated file
				unsigned int in_uiBufferSize,	///< Size of the provided string buffer
				unsigned int in_Index = 0		///< Optional index
				) const = 0;

			/// Retrieve the file path of the source plug-in data at the specified index.
			/// \return Number of characters written to the buffer, zero if failed.
			virtual unsigned int GetMediaSourceOriginalFilePath(
				LPWSTR out_pszFileName,			///< Relative path of the associated file
				unsigned int in_uiBufferSize,	///< Size of the provided string buffer
				unsigned int in_Index = 0		///< Optional index
				) const = 0;

			/// Retrieve the file path of the converted plug-in data at the specified index.
			/// \return Number of characters written to the buffer, zero if failed.
			virtual unsigned int GetMediaSourceConvertedFilePath(
				LPWSTR out_pszFileName,			///< Relative path of the associated file
				unsigned int in_uiBufferSize,	///< Size of the provided string buffer
				const GUID & in_guidPlatform,	///< The GUID of the platform
				unsigned int in_Index = 0		///< Optional index
				) const = 0;

			/// Request Wwise to perform any required conversion on the data
			virtual void InvalidateMediaSource( unsigned int in_Index = 0 ) = 0;

			/// Obtain the Original directory for the plugin
			/// \return Number of characters written to the buffer, zero if failed.
			virtual unsigned int GetOriginalDirectory(
				LPWSTR out_pszDirectory,		///< Pointer to the buffer that will hold the directory string
				unsigned int in_uiBufferSize	///< Size of the buffer pointed by out_pszDirectory
				) const = 0;

			/// Obtain the Converted directory for the plugin and platform
			/// \return Number of characters written to the buffer, zero if failed.
			virtual unsigned int GetConvertedDirectory(
				LPWSTR out_pszDirectory,		///< Pointer to the buffer that will hold the directory string
				unsigned int in_uiBufferSize,	///< Size of the buffer pointed by out_pszDirectory
				const GUID & in_guidPlatform	///< The GUID of the platform
				) const = 0;
		};

		/// Wwise plug-in interface. This must be implemented for each source or
		/// effect plug-in that is exposed in Wwise.
		/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
		/// \sa
		/// - \ref wwiseplugin_object
		class IAudioPlugin
			: public IPluginBase
		{
		public:
			/// Dialog type. Source plug-ins can be edited in the Property Editor or
			/// the Contents Editor, while effect plug-ins can only be edited in the
			/// Effect Editor.
			/// \sa
			/// - \ref wwiseplugin_dialogcode
			enum eDialog
			{
				SettingsDialog,			///< Main plug-in dialog. This is the dialog used in the Property
										///< Editor for source plug-ins, and in the Effect Editor for
										///< effect plug-ins.
				ContentsEditorDialog	///< Contents Editor dialog. This is the small dialog used in the
										///< Contents Editor for source plug-ins.
			};

			/// Type of operation for the NotifyInnerObjectAddedRemoved function.
			enum NotifyInnerObjectOperation
			{
				InnerObjectAdded,
				InnerObjectRemoved
			};

			/// The property set interface is given to the plug-in through this method. It is called by Wwise during
			/// initialization of the plug-in, before most other calls.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \sa
			/// - \ref wwiseplugin_propertyset
			virtual void SetPluginPropertySet( 
				IPluginPropertySet * in_pPSet	///< A pointer to the property set interface
				) = 0;

			/// The plugin object store interface is given to the plug-in through this method. 
			/// It is called by Wwise during initialization of the plug-in, before most other calls.
			/// Use this interface to manage plugin inner objects.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \sa
			/// - \ref wwiseplugin_objectstore
			virtual void SetPluginObjectStore( 
				IPluginObjectStore * in_pObjectStore ///< A pointer to the plugin object store
				) = 0;

			/// The plugin object data file interface is given to the plug-in through this method. 
			/// Set plugin object media, that allows to create and manage media files
			/// Use this interface to manage plugin media objects.
			///
			/// NOTE: If the plug-in does not handle plugin media, this function should be 
			/// implemented as a void function by the plug-in.
			///
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \sa
			/// - \ref effectplugin_media
			virtual void SetPluginObjectMedia( 
				IPluginObjectMedia * in_pObjectMedia
				) = 0;

			/// This function is called by Wwise to determine if the plug-in is in a playable state.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \return True if the plug-in is in a playable state, False otherwise
			virtual bool IsPlayable() const = 0;

			/// Initialize custom data to default values. This is called by Wwise after SetPluginPropertySet() 
			/// when creating a new instance of the plug-in (i.e. not during a load). The properties on the
			/// PropertySet do not need to be initialized in this method.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			virtual void InitToDefault() = 0;

			/// Delete function called when the user press "delete" button on a plugin. This entry point must 
			/// set the undo/redo action properly. 
			/// \warning This function is guaranteed to be called by a single thread at a time.
			virtual void Delete() = 0;

			/// Load file 
			/// \return \b true if load succeeded.
			virtual bool Load( IXmlTextReader* in_pReader ) = 0;

			/// Save file
			/// \return \b true if save succeeded.
			virtual bool Save( IXmlTextWriter* in_pWriter ) = 0;

			/// Copy the plugin's custom data into another instance of the same plugin. This is used
			/// during copy/paste and delete. The properties on the PropertySet do not need to
			/// be copied in this method.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			virtual bool CopyInto(
				IAudioPlugin* io_pWObject		 // The object that will receive the custom data of this object.
				) const = 0;

			/// This function is called by Wwise when the current platform changes.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \sa
			/// - \ref wwiseplugin_platformchange
			virtual void NotifyCurrentPlatformChanged( 
				const GUID & in_guidCurrentPlatform		///< The unique ID of the new platform
				) = 0;

			/// This function is called by Wwise when a plug-in property changes (for example, 
			/// through interaction with a UI control bound to a property, or through undo/redo operations).
			/// This function is also called during undo or redo operations
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \sa
			/// - \ref wwiseplugin_propertychange
			virtual void NotifyPropertyChanged( 
				const GUID & in_guidPlatform,	///< The unique ID of the queried platform
				LPCWSTR in_pszPropertyName		///< The name of the property
				) = 0;

			/// This function is called by Wwise when a inner object property changes (for example, 
			/// through interaction with a UI control bound to a property, or through undo/redo operations).
			/// See the Plugin Object Store for more information about inner objects.
			/// This function is also called during undo or redo operations
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \sa
			/// - \ref wwiseplugin_propertychange
			virtual void NotifyInnerObjectPropertyChanged( 
				IPluginPropertySet* in_pPSet,	///< The inner object that changed
				const GUID & in_guidPlatform,	///< The unique ID of the queried platform
				LPCWSTR in_pszPropertyName		///< The name of the property
				) = 0;

			/// This function is called by Wwise when a inner object property changes (for example, 
			/// through interaction with a UI control bound to a property, or through undo/redo operations).
			/// See the Plugin Object Store for more information about inner objects.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \sa
			/// - \ref wwiseplugin_propertychange
			virtual void NotifyInnerObjectAddedRemoved( 
				IPluginPropertySet* in_pPSet,	///< The inner object that was added or removed
				unsigned int in_uiIndex,		///< The insertion/removal index
				NotifyInnerObjectOperation in_eOperation	///< InnerObjectAdded or InnerObjectRemoved
				) = 0;

			/// This function is called by Wwise when a the plugin media changes.
			/// It is called when plugin media is added, removed or changes.
			/// This function is also called during undo or redo operations
			/// Use AK::Wwise::IAudioPlugin::SetPluginObjectMedia and AK::Wwise::IPluginObjectMedia to
			/// set plugin media.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \sa
			/// - \ref wwiseplugin_propertychange
			virtual void NotifyPluginMediaChanged() = 0;

            /// This function is called by Wwise to obtain parameters that will be written to a bank. 
			/// Because these can be changed at run-time, the parameter block should stay relatively small. 
			/// Larger data should be put in the Data Block.
			/// \warning This function is guaranteed to be called by a single thread at a time.
            /// \return True if the plug-in put some parameters in the bank, False otherwise
			/// \sa
			/// - \ref wwiseplugin_bank
			/// - \ref wwiseplugin_propertyset
            virtual bool GetBankParameters( 
				const GUID & in_guidPlatform,	///< The unique ID of the queried platform
				IWriteData* in_pDataWriter		///< A pointer to the data writer interface
				) const = 0;

			/// This function is called by Wwise to obtain parameters that will be sent to the 
			/// sound engine when Wwise is connected.  This block should contain only data
			/// that is NOT a property defined in the plugin xml file.  The parameter ID
			/// should be something different than the ones used in the plugin xml.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \return True if the plug-in has some plugin-defined data.  False otherwise.
			/// \sa
			/// - AK::Wwise::IPluginPropertySet::NotifyInternalDataChanged
			/// - AK::IAkPluginParam::ALL_PLUGIN_DATA_ID
			/// - AK::IAkPluginParam::SetParam
			virtual bool GetPluginData(
				const GUID & in_guidPlatform,		///< The unique ID of the queried platform
				AkPluginParamID in_idParam,	///< The plugin-defined parameter ID
				IWriteData* in_pDataWriter			///< A pointer to the data writer interface
				) const = 0;

			/// This function is called by Wwise to get the plug-in's HINSTANCE used for loading resources.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \return A handle to the instance of the plug-in DLL
			/// \sa
			/// - \ref wwiseplugin_dialogcode
			virtual HINSTANCE GetResourceHandle() const = 0;

			/// This function is called by Wwise to get the plug-in dialog parameters.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \return True if a dialog was returned, False otherwise
			/// \sa
			/// - \ref wwiseplugin_dialogcode
			/// - \ref wwiseplugin_dialog_guide
			virtual bool GetDialog( 
				eDialog in_eDialog,				///< The dialog type
				UINT & out_uiDialogID,			///< The returned resource ID of the dialog
				PopulateTableItem *& out_pTable	///< The returned table of property-control bindings (can be NULL)
				) const = 0;

			/// Window message handler for dialogs. This is very similar to a standard WIN32 window procedure.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \return True if the message has been processed by the plug-in, False otherwise
			/// \sa
			/// - \ref wwiseplugin_dialogcode
			virtual bool WindowProc( 
				eDialog in_eDialog,		///< The dialog type
				HWND in_hWnd,			///< The window handle of the dialog
				UINT in_message,		///< The incoming message. This is a standard Windows message ID (ex. WM_PAINT).
				WPARAM in_wParam,		///< The WPARAM of the message (see MSDN)
				LPARAM in_lParam,		///< The LPARAM of the message (see MSDN)
				LRESULT & out_lResult 	///< The returned value if the message has been processed (it is only considered if the method also returns True)
				) = 0;

			/// DEPRECATED: This function is called by Wwise to get the user-friendly name of the specified property.
			/// This function should write the user-friendly name of
			/// the specified property to the WCHAR buffer out_pszDisplayName,
			/// which is of length in_unCharCount.
			/// \warning This function is deprecated.  You need to define the property display names in the plug-in XML definition.  Refer to \ref wwiseplugin_xml_userinterface for more information.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \return True if the property has a user-friendly name, False otherwise
			/// \sa
			/// - \ref wwiseplugin_displaynames
			virtual bool DisplayNameForProp( 
				LPCWSTR in_pszPropertyName,		///< The internal name of the property
				LPWSTR out_pszDisplayName,		///< The returned user-friendly name
				UINT in_unCharCount				///< The number of WCHAR in the buffer, including the terminating NULL
				) const = 0;

			/// DEPRECATED: This function is called by Wwise to get the user-friendly names of possible values for the 
			/// specified property.
			/// This function should write pairs of value and text for the specified property to
			/// the WCHAR buffer out_pszDisplayName, which is of length in_unCharCount.
			/// Pairs are separated by commas, and each pair contains the value and the
			/// text, separated by a colon. Here are a few examples:
			/// - Numeric property: "-100:Left,0:Center,100:Right"
			/// - Boolean property: "0:Off,1:On"
			/// - Numeric property seen as an enumeration: "0:Low Pass,1:High Pass,2:Band Pass,3:Notch,4:Low Shelf,5:High Shelf,6:Peaking"
			///
			/// \warning This function is deprecated.  You need to define the enumeration display names in the plug-in XML definition.  Refer to \ref wwiseplugin_xml_restrictions for more information.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \return True if the property has user-friendly names for some values, False otherwise
			/// \sa
			/// - \ref wwiseplugin_displaynames
			virtual bool DisplayNamesForPropValues( 
				LPCWSTR in_pszPropertyName,		///< The internal name of the property
				LPWSTR out_pszValuesName,		///< The returned property value names
				UINT in_unCharCount				///< The number of WCHAR in the buffer, including the terminating NULL character
				) const = 0;

			/// Called when the user clicks on the '?' icon.
			/// \warning This function is guaranteed to be called by a single thread at a time.
			/// \return True if the plug-in handled the help request, false otherwise
			/// \sa
			/// - \ref wwiseplugin_help
			virtual bool Help( 
				HWND in_hWnd,					///< The handle of the dialog
				eDialog in_eDialog,				///< The dialog type
				LPCWSTR in_szLanguageCode		///< The language code in ISO639-1
				) const = 0;

			/// Called when an instance of the run-time component of the plug-in sends data 
			/// using AK::IAkEffectPluginContext::PostMonitorData(), and this plug-in's settings 
			/// are being displayed in a window.
			virtual void NotifyMonitorData( 
				void * in_pData, 				///< Blob of data
				unsigned int in_uDataSize, 		///< Size of data
				bool in_bNeedsByteSwap			///< True if data comes from platform with a different byte ordering (i.e. Big Endian)
				) = 0;

			/// Retrieve a pointer to the class implementing IPluginObjectMedia. Plug-ins using the media sources
			/// functionality can simply return a pointer to themselves while other not using the functionality should return NULL
			virtual IPluginMediaConverter* GetPluginMediaConverterInterface() = 0;

			/// Retrieve the licensing status of the plug-in for the given platform.
			/// \return True if the license is valid, False if the license is expired or not present (the plug-in will then be omitted from SoundBanks).
			/// \sa
			/// - \ref IPluginPropertySet::GetAssetLicenseStatus
			virtual bool GetLicenseStatus(
				const GUID & in_guidPlatform,		///< GUID of the platform
				AK::Wwise::Severity& out_eSeverity,	///< (Optional) If set, the string placed in out_pszMessage will be shown in the log with the corresponding severity. 
				LPWSTR out_pszMessage,				///< Pointer to the buffer that will hold the message string
				unsigned int in_uiBufferSize		///< Size of the buffer pointed by out_pszMessage (in number of WCHAR, including null terminator)
				) = 0;
		};

		/// Use this base class to quickly implement most plugin functions empty
		class DefaultAudioPluginImplementation : public IAudioPlugin
		{
		public:
			virtual void SetPluginPropertySet( IPluginPropertySet * in_pPSet ){}
			virtual void SetPluginObjectStore( IPluginObjectStore * in_pObjectStore ){}
			virtual void SetPluginObjectMedia( IPluginObjectMedia * in_pObjectMedia ){}
			virtual bool IsPlayable() const { return true; }
			virtual void InitToDefault() {}
			virtual void Delete() {}
			virtual bool Load( IXmlTextReader* in_pReader ) { return false; }
			virtual bool Save( IXmlTextWriter* in_pWriter ) { return false; }
			virtual bool CopyInto( IAudioPlugin* io_pWObject ) const { return true; }
			virtual void NotifyCurrentPlatformChanged( const GUID & in_guidCurrentPlatform ) {}
			virtual void NotifyPropertyChanged( const GUID & in_guidPlatform, LPCWSTR in_pszPropertyName ) {}
			virtual void NotifyInnerObjectPropertyChanged( IPluginPropertySet* in_pPSet, const GUID & in_guidPlatform, LPCWSTR in_pszPropertyName ) {}
			virtual void NotifyInnerObjectAddedRemoved( IPluginPropertySet* in_pPSet, unsigned int in_uiIndex, NotifyInnerObjectOperation in_eOperation	) {}
			virtual void NotifyPluginMediaChanged() {}
			virtual bool GetBankParameters( const GUID & in_guidPlatform, IWriteData* in_pDataWriter ) const { return false; }
			virtual bool GetPluginData( const GUID & in_guidPlatform, AkPluginParamID in_idParam, IWriteData* in_pDataWriter ) const { return false; }
			virtual bool WindowProc( eDialog in_eDialog, HWND in_hWnd, UINT in_message, WPARAM in_wParam, LPARAM in_lParam, LRESULT & out_lResult ){ return false; }
			virtual bool DisplayNameForProp( LPCWSTR in_pszPropertyName, LPWSTR out_pszDisplayName, UINT in_unCharCount	) const { return false; }
			virtual bool DisplayNamesForPropValues( LPCWSTR in_pszPropertyName,	LPWSTR out_pszValuesName, UINT in_unCharCount ) const { return false; }
			virtual bool Help( HWND in_hWnd, eDialog in_eDialog, LPCWSTR in_szLanguageCode ) const { return false; }
			virtual void NotifyMonitorData( void * in_pData, unsigned int in_uDataSize, bool in_bNeedsByteSwap ){}
			virtual IPluginMediaConverter* GetPluginMediaConverterInterface() { return NULL; }
			virtual bool GetLicenseStatus( const GUID &, AK::Wwise::Severity&, LPWSTR, unsigned int in_uiBufferSize ){ return true; }
		};
	}
}

/// Dummy assert hook for Wwise plug-ins using AKASSERT (cassert used by default).
#ifdef _DEBUG
#define DEFINEDUMMYASSERTHOOK AkAssertHook g_pAssertHook = NULL;
#else
#define DEFINEDUMMYASSERTHOOK 
#endif

/// Private message sent to Wwise window to open a topic in the help file
/// the WPARAM defines the help topic ID
#define WM_AK_PRIVATE_SHOW_HELP_TOPIC	0x4981

#endif // _AK_WWISE_AUDIOPLUGIN_H
