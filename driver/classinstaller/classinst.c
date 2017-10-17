#include <DriverSpecs.h>
__user_code  

#include <windows.h>
#include <strsafe.h>
#include <setupapi.h>
#include <stdio.h>
#include "resource.h"
#include <dontuse.h>

#if DBG
#define DbgOut(Text) OutputDebugString(TEXT("ClassInstaller: " Text "\n"))
#else
#define DbgOut(Text) 
#endif 

typedef struct _TOASTER_PROP_PARAMS
{

   HDEVINFO                     DeviceInfoSet;
   PSP_DEVINFO_DATA             DeviceInfoData;
   BOOL                         Restart;
   
} TOASTER_PROP_PARAMS, *PTOASTER_PROP_PARAMS;


INT_PTR PropPageDlgProc(_In_ HWND hDlg, _In_ UINT uMessage, _In_ WPARAM wParam, _In_ LPARAM lParam);
UINT CALLBACK PropPageDlgCallback(HWND hwnd, UINT uMsg, LPPROPSHEETPAGE ppsp);
DWORD PropPageProvider(_In_ HDEVINFO DeviceInfoSet, _In_ PSP_DEVINFO_DATA DeviceInfoData OPTIONAL);
BOOL OnNotify(HWND ParentHwnd, LPNMHDR NmHdr, PTOASTER_PROP_PARAMS Params);
    
HMODULE ModuleInstance;

BOOL WINAPI 
DllMain(
    HINSTANCE DllInstance,
    DWORD Reason,
    PVOID Reserved
    )
{

    UNREFERENCED_PARAMETER( Reserved );

    switch(Reason) {

        case DLL_PROCESS_ATTACH: {

            ModuleInstance = DllInstance;
            DisableThreadLibraryCalls(DllInstance);
            InitCommonControls();
            break;
        }

        case DLL_PROCESS_DETACH: {
            ModuleInstance = NULL;
            break;
        }

        default: {
            break;
        }
    }

    return TRUE;
}

DWORD CALLBACK
SynccomClassInstaller(
    _In_  DI_FUNCTION         InstallFunction,
    _In_  HDEVINFO            DeviceInfoSet,
    _In_  PSP_DEVINFO_DATA    DeviceInfoData OPTIONAL
    )
{
    switch (InstallFunction)
    {
        case DIF_NEWDEVICEWIZARD_FINISHINSTALL: {
			ULONG ui_number;
			char name[30];
			
			SetupDiGetDeviceRegistryProperty(DeviceInfoSet, 
				DeviceInfoData,
				SPDRP_UI_NUMBER,
				NULL,
				(PBYTE)&ui_number,
				sizeof(ui_number),
				NULL
			);

			memset(name, 0, sizeof(name));
			sprintf_s(name, sizeof(name), "SYNCCOM Port (SYNCCOM%i)", ui_number);

			SetupDiSetDeviceRegistryProperty(DeviceInfoSet, 
				DeviceInfoData,
				SPDRP_FRIENDLYNAME,
				(BYTE *)name,
				(DWORD)strlen(name)
			);
		}
			break;
        default:
            break;
    }   
    return ERROR_DI_DO_DEFAULT;    
}

DWORD
PropPageProvider(
    _In_  HDEVINFO            DeviceInfoSet,
    _In_  PSP_DEVINFO_DATA    DeviceInfoData OPTIONAL
)
{
    HPROPSHEETPAGE  pageHandle;
    PROPSHEETPAGE   page;
    PTOASTER_PROP_PARAMS      params = NULL;
    SP_ADDPROPERTYPAGE_DATA AddPropertyPageData = {0};

    //
    // DeviceInfoSet is NULL if setup is requesting property pages for
    // the device setup class. We don't want to do anything in this 
    // case.
    //
    if (DeviceInfoData==NULL) {
        return ERROR_DI_DO_DEFAULT;
    }

    AddPropertyPageData.ClassInstallHeader.cbSize = 
         sizeof(SP_CLASSINSTALL_HEADER);

    //
    // Get the current class install parameters for the device
    //

    if (SetupDiGetClassInstallParams(DeviceInfoSet, DeviceInfoData,
         (PSP_CLASSINSTALL_HEADER)&AddPropertyPageData,
         sizeof(SP_ADDPROPERTYPAGE_DATA), NULL )) 
    {

        //
        // Ensure that the maximum number of dynamic pages for the 
        // device has not yet been met
        //
        if(AddPropertyPageData.NumDynamicPages >= MAX_INSTALLWIZARD_DYNAPAGES){
            return NO_ERROR;
        }
        params = HeapAlloc(GetProcessHeap(), 0, sizeof(TOASTER_PROP_PARAMS));
        if (params)
        {
            //
            // Save DeviceInfoSet and DeviceInfoData
            //
            params->DeviceInfoSet     = DeviceInfoSet;
            params->DeviceInfoData    = DeviceInfoData;
            params->Restart           = FALSE;
            
            //
            // Create custom property sheet page
            //
            memset(&page, 0, sizeof(PROPSHEETPAGE));

            page.dwSize = sizeof(PROPSHEETPAGE);
            page.dwFlags = PSP_USECALLBACK;
            page.hInstance = ModuleInstance;
            page.pszTemplate = MAKEINTRESOURCE(DLG_TOASTER_PORTSETTINGS);
            page.pfnDlgProc = (DLGPROC) PropPageDlgProc;
            page.pfnCallback = PropPageDlgCallback;

            page.lParam = (LPARAM) params;

            pageHandle = CreatePropertySheetPage(&page);
            if(!pageHandle)
            {
                HeapFree(GetProcessHeap(), 0, params);
                return NO_ERROR;
            }

            //
            // Add the new page to the list of dynamic property 
            // sheets
            //
            AddPropertyPageData.DynamicPages[
                AddPropertyPageData.NumDynamicPages++]=pageHandle;

            SetupDiSetClassInstallParams(DeviceInfoSet,
                        DeviceInfoData,
                        (PSP_CLASSINSTALL_HEADER)&AddPropertyPageData,
                        sizeof(SP_ADDPROPERTYPAGE_DATA));
        }
    }
    return NO_ERROR;
} 

INT_PTR
PropPageDlgProc(_In_ HWND   hDlg,
                   _In_ UINT   uMessage,
                   _In_ WPARAM wParam,
                   _In_ LPARAM lParam)
{
    PTOASTER_PROP_PARAMS params;

    UNREFERENCED_PARAMETER( wParam );

    params = (PTOASTER_PROP_PARAMS) GetWindowLongPtr(hDlg, DWLP_USER);

    switch(uMessage) {
    case WM_COMMAND:
        break;

    case WM_CONTEXTMENU:
        break;

    case WM_HELP:
        break;

    case WM_INITDIALOG:

        //
        // on WM_INITDIALOG call, lParam points to the property
        // sheet page.
        //
        // The lParam field in the property sheet page struct is set by the
        // caller. This was set when we created the property sheet.
        // Save this in the user window long so that we can access it on later 
        // on later messages.
        //

        params = (PTOASTER_PROP_PARAMS) ((LPPROPSHEETPAGE)lParam)->lParam;
        SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR) params);
        break;


    case WM_NOTIFY:
        OnNotify(hDlg, (NMHDR *)lParam, params);
        break;

    default: 
        return FALSE;
    }

    return TRUE;
} 


UINT
CALLBACK
PropPageDlgCallback(HWND hwnd,
                   UINT uMsg,
                   LPPROPSHEETPAGE ppsp)
{
    PTOASTER_PROP_PARAMS params;

    UNREFERENCED_PARAMETER( hwnd );

    switch (uMsg) {

    case PSPCB_CREATE:
        //
        // Called when the property sheet is first displayed
        //
        return TRUE;    // return TRUE to continue with creation of page

    case PSPCB_RELEASE:
        //
        // Called when property page is destroyed, even if the page 
        // was never displayed. This is the correct way to release data.
        //
        params = (PTOASTER_PROP_PARAMS) ppsp->lParam;
        LocalFree(params);
        return 0;       // return value ignored
    default:
        break;
    }

    return TRUE;
}


BOOL
OnNotify(
    HWND    ParentHwnd,
    LPNMHDR NmHdr,
    PTOASTER_PROP_PARAMS Params
    )
{
    SP_DEVINSTALL_PARAMS spDevInstall = {0};
    TCHAR                friendlyName[LINE_LEN] ={0};
    size_t  charCount;
    BOOL    fSuccess;

    switch (NmHdr->code) {
    case PSN_APPLY:
        //
        // Sent when the user clicks on Apply OR OK !!
        //

        GetDlgItemText(ParentHwnd, IDC_FRIENDLYNAME, friendlyName,
                                        LINE_LEN-1 );
        friendlyName[LINE_LEN-1] = UNICODE_NULL;
        if(friendlyName[0]) {

            if (FAILED(StringCchLength(friendlyName,
                         STRSAFE_MAX_CCH,
                         &charCount))) {
                DbgOut("StringCchLength failed!");                   
                break;
            }
            fSuccess = SetupDiSetDeviceRegistryProperty(Params->DeviceInfoSet, 
                         Params->DeviceInfoData,
                         SPDRP_FRIENDLYNAME,
                         (BYTE *)friendlyName,
                         (DWORD)((charCount + 1) * sizeof(TCHAR))
                         );
            if(!fSuccess) {
                DbgOut("SetupDiSetDeviceRegistryProperty failed!");                   
                break;
            }

            //
            // Inform setup about property change so that it can
            // restart the device.
            //

            spDevInstall.cbSize = sizeof(SP_DEVINSTALL_PARAMS);
     
            if (Params && SetupDiGetDeviceInstallParams(Params->DeviceInfoSet,
                                              Params->DeviceInfoData,
                                              &spDevInstall)) {
                //
                // If your device requires a reboot to restart, you can
                // specify that by setting DI_NEEDREBOOT as shown below
                //
                // if(NeedReboot) {
                //    spDevInstall.Flags |= DI_PROPERTIES_CHANGE | DI_NEEDREBOOT;
                // }
                //
                spDevInstall.FlagsEx |= DI_FLAGSEX_PROPCHANGE_PENDING;
                
                SetupDiSetDeviceInstallParams(Params->DeviceInfoSet,
                                              Params->DeviceInfoData,
                                              &spDevInstall);
            }
        
        }
        return TRUE;

    default:
        return FALSE;
    }
    return FALSE;   
} 

