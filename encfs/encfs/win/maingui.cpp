#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <stdio.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdexcept>
#include "guiutils.h"
#include "drives.h"
#include "resource.h"
#include "FileUtils.h"
#include "Cipher.h"
#include "BlockNameIO.h"

// TODO preference, start at login

NOTIFYICONDATA niData;

static ULONGLONG GetDllVersion(LPCTSTR lpszDllName);
static BOOL InitInstance(HINSTANCE hInstance, int nCmdShow);
static INT_PTR CALLBACK MainDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK OptionsDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

#define TRAYICONID	1
#define SWM_TRAYMSG	WM_APP+100

static void SetPath()
{
	char path[MAX_PATH];
	if (!GetModuleFileName(GetModuleHandle(NULL), path, sizeof(path)))
		return;
	char *p = strrchr(path, '\\');
	if (!p)
		return;
	*p = 0;
	SetCurrentDirectory(path);
}

extern "C" int main_gui(HINSTANCE /* hInstance */, HINSTANCE /* hPrevInstance */ , LPSTR /* lpCmdLine */ ,int nCmdShow)
{
	MSG msg;

	// TODO check Dokan version

	SetPath();

	if (!InitInstance(hInst, nCmdShow))
		return FALSE;

	while (GetMessage(&msg, NULL, 0, 0)) {
		if (!IsDialogMessage(msg.hwnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int) msg.wParam;
}

// Initialize the window and tray icon
static BOOL
InitInstance(HINSTANCE hInstance, int /* nCmdShow */)
{
	// prepare for XP style controls
	InitCommonControls();

	HWND hWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC) MainDlgProc);
	if (!hWnd)
		return FALSE;

	ZeroMemory(&niData, sizeof(NOTIFYICONDATA));

	ULONGLONG ullVersion = GetDllVersion(_T("shell32.dll"));
	if (ullVersion >= MAKEDLLVERULL(5, 0, 0, 0))
		niData.cbSize = sizeof(NOTIFYICONDATA);
	else
		niData.cbSize = NOTIFYICONDATA_V2_SIZE;

	// the ID number can be anything you choose
	niData.uID = TRAYICONID;

	// state which structure members are valid
	niData.uFlags = NIF_ICON | NIF_MESSAGE;

	// load the icon
	niData.hIcon =
		(HICON) LoadImage(hInstance, MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
				  GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

	// the window to send messages to and the message to send
	//              note:   the message value should be in the
	//                              range of WM_APP through 0xBFFF
	niData.hWnd = hWnd;
	niData.uCallbackMessage = SWM_TRAYMSG;

	Shell_NotifyIcon(NIM_ADD, &niData);

	// free icon handle
	if (niData.hIcon && DestroyIcon(niData.hIcon))
		niData.hIcon = NULL;

	// call ShowWindow here to make the dialog initially visible
	return TRUE;
}

// Get dll version number
static ULONGLONG
GetDllVersion(LPCTSTR lpszDllName)
{
	DLLGETVERSIONPROC pDllGetVersion =
		(DLLGETVERSIONPROC) GetProcAddress(GetModuleHandle(lpszDllName), "DllGetVersion");
	if (!pDllGetVersion)
		return 0;

	DLLVERSIONINFO dvi;
	HRESULT hr;

	ZeroMemory(&dvi, sizeof(dvi));
	dvi.cbSize = sizeof(dvi);
	hr = (*pDllGetVersion) (&dvi);
	if (SUCCEEDED(hr))
		return MAKEDLLVERULL(dvi.dwMajorVersion, dvi.dwMinorVersion, 0, 0);

	return 0;
}

static const int NormalKDFDuration = 500;
static const int DefaultBlockSize = 1024;
static const int ParanoiaKDFDuration = 3000; // 3 seconds
const int V6SubVersion = 20100713; // add version field for boost 1.42+

static
Cipher::CipherAlgorithm findCipherAlgorithm(const char *name,
	int keySize )
{
	Cipher::AlgorithmList algorithms = Cipher::GetAlgorithmList();
	Cipher::AlgorithmList::const_iterator it;
	for(it = algorithms.begin(); it != algorithms.end(); ++it)
	{
		if (!strcmp( name, it->name.c_str() )
		    && it->keyLength.allowed( keySize ))
			return *it;
	}

	Cipher::CipherAlgorithm result;
	return result;
}


static void createConfig(const std::string& rootDir, bool paranoid, const char* password)
{
	bool reverseEncryption = false;
	ConfigMode configMode = paranoid ? Config_Paranoia : Config_Standard;
    
	int keySize = 0;
	int blockSize = 0;
	Cipher::CipherAlgorithm alg;
	rel::Interface nameIOIface;
	int blockMACBytes = 0;
	int blockMACRandBytes = 0;
	bool uniqueIV = false;
	bool chainedIV = false;
	bool externalIV = false;
	bool allowHoles = true;
	long desiredKDFDuration = NormalKDFDuration;
    
	if (reverseEncryption)
	{
		uniqueIV = false;
		chainedIV = false;
		externalIV = false;
		blockMACBytes = 0;
		blockMACRandBytes = 0;
	}

	if(configMode == Config_Paranoia)
	{
		// look for AES with 256 bit key..
		// Use block filename encryption mode.
		// Enable per-block HMAC headers at substantial performance penalty..
		// Enable per-file initialization vector headers.
		// Enable filename initialization vector chaning
		keySize = 256;
		blockSize = DefaultBlockSize;
		alg = findCipherAlgorithm("AES", keySize);
		nameIOIface = BlockNameIO::CurrentInterface();
		blockMACBytes = 8;
		blockMACRandBytes = 0; // using uniqueIV, so this isn't necessary
		uniqueIV = true;
		chainedIV = true;
		externalIV = true;
		desiredKDFDuration = ParanoiaKDFDuration;
	} else {
		// xgroup(setup)
		// AES w/ 192 bit key, block name encoding, per-file initialization
		// vectors are all standard.
		keySize = 192;
		blockSize = DefaultBlockSize;
		alg = findCipherAlgorithm("AES", keySize);
		blockMACBytes = 0;
		externalIV = false;
		nameIOIface = BlockNameIO::CurrentInterface();

		if (!reverseEncryption)
		{
			uniqueIV = true;
			chainedIV = true;
		}
	}

	shared_ptr<Cipher> cipher = Cipher::New( alg.name, keySize );
	if(!cipher)
	{
		char buf[256];
		_snprintf(buf, sizeof(buf), "Unable to instanciate cipher %s, key size %i, block size %i",
			alg.name.c_str(), keySize, blockSize);
		throw std::runtime_error(buf);
	}
    
	shared_ptr<EncFSConfig> config( new EncFSConfig );

	config->cfgType = Config_V6;
	config->cipherIface = cipher->interface();
	config->keySize = keySize;
	config->blockSize = blockSize;
	config->nameIface = nameIOIface;
	config->creator = "EncFS " VERSION;
	config->subVersion = V6SubVersion;
	config->blockMACBytes = blockMACBytes;
	config->blockMACRandBytes = blockMACRandBytes;
	config->uniqueIV = uniqueIV;
	config->chainedNameIV = chainedIV;
	config->externalIVChaining = externalIV;
	config->allowHoles = allowHoles;

	config->salt.clear();
	config->kdfIterations = 0; // filled in by keying function
	config->desiredKDFDuration = desiredKDFDuration;

	int encodedKeySize = cipher->encodedKeySize();
	unsigned char *encodedKey = new unsigned char[ encodedKeySize ];

	CipherKey volumeKey = cipher->newRandomKey();

	// get user key and use it to encode volume key
	CipherKey userKey;
	userKey = config->makeKey(password, strlen(password));

	cipher->writeKey( volumeKey, encodedKey, userKey );
	userKey.reset();

	config->assignKeyData(encodedKey, encodedKeySize);
	delete[] encodedKey;

	if(!volumeKey)
		throw std::runtime_error("Failure generating new volume key! "
		                         "Please report this error.");

	if (!saveConfig( Config_V6, rootDir, config ))
		throw std::runtime_error("Error saving configuration file");
}

struct OptionsData
{
	OptionsData():paranoia(false),drive(0) { password[0] = 0; }
	~OptionsData() { memset(password, 0, sizeof(password)); }
	std::string rootDir;
	bool paranoia;
	char drive;
	char password[128];
};

static BOOL
OnInitDialog(HWND hWnd, OptionsData& data)
{
	FillFreeDrive(GetDlgItem(hWnd, IDC_CMBDRIVE));

	SetDlgItemText(hWnd, IDC_DIR, data.rootDir.c_str());

	if (data.paranoia)
		SendDlgItemMessage(hWnd, IDC_CHKPARANOIA, BM_SETCHECK, BST_CHECKED, 0);

	HICON hIcon;

	hIcon = (HICON) LoadImage(hInst, MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
				GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM) hIcon);

	hIcon = (HICON) LoadImage(hInst, MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
				GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM) hIcon);
	return TRUE;
}

static void
ShowContextMenu(HWND hWnd)
{
	POINT pt;

	GetCursorPos(&pt);
	HMENU hMenu = CreatePopupMenu();

	if (!hMenu)
		return;

	AppendMenu(hMenu, 0, IDM_OPEN, _T("Open/Create"));
	AppendMenu(hMenu, MF_MENUBREAK, -1, NULL);
	Drives::AddMenus(hMenu);
	AppendMenu(hMenu, 0, IDM_ABOUT, _T("About"));
	AppendMenu(hMenu, 0, IDM_EXIT, _T("Exit"));
	SetMenuDefaultItem(hMenu, IDM_OPEN, FALSE);

	// avoid disappear
	SetForegroundWindow(hWnd);
	TrackPopupMenu(hMenu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
	DestroyMenu(hMenu);
}

static
std::string slashTerminate(const std::string &src)
{
	std::string result = src;
	if (result[ result.length()-1 ] != '/')
		result.append( "/" );
	return result;
}

static void
OpenOrCreate(HWND hwnd)
{
	static bool openAlreadyOpened = false;

	if (openAlreadyOpened)
		return;

	class Unique {
	public:
		Unique() { openAlreadyOpened = true; }
		~Unique() { openAlreadyOpened = false; }
	} unique;

	std::string dir = GetExistingDirectory(hwnd, "Select a folder which contains or will contain encrypted data.", "Select Crypt Folder");
	if (dir.empty())
		return;

	// if directory is already configured add and try to mount
	boost::shared_ptr<EncFSConfig> config(new EncFSConfig);
	if (readConfig(slashTerminate(dir), config) != Config_None) {
		char drive = SelectFreeDrive(hwnd);
		if (drive) {
			Drives::drive_t dr(Drives::Add(dir, drive));
			if (dr)
				dr->Mount(hwnd);
		}
		return;
	}

	// TODO check directory is empty, warning if continue
	// "You are initializing a crypted directory with a no-empty directory. Is this expected?"
	OptionsData data;
	data.rootDir = dir;
	if (DialogBoxParam(hInst, (LPCTSTR) IDD_OPTIONS, hwnd, (DLGPROC) OptionsDlgProc, (LPARAM) &data) != IDOK)
		return;

	// add configuration and add new drive
	createConfig(slashTerminate(dir), data.paranoia, data.password);

	Drives::drive_t dr(Drives::Add(dir, data.drive));
	if (dr)
		dr->Mount(hwnd);
}

// Message handler for the app
static INT_PTR CALLBACK
OptionsDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	OptionsData *pData = (OptionsData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
	char buf1[sizeof(pData->password)];
	char buf2[sizeof(pData->password)];
	bool diff;
	int i;

	switch (message) {
	case WM_INITDIALOG:
		pData = (OptionsData*) lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
		return OnInitDialog(hWnd, *pData);
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			EndDialog(hWnd, LOWORD(wParam));
			return TRUE;
		case IDOK:
			pData->paranoia = (IsDlgButtonChecked(hWnd, IDC_CHKPARANOIA) == BST_CHECKED);
			GetDlgItemText(hWnd, IDC_PWD1, buf1, sizeof(buf1));
			GetDlgItemText(hWnd, IDC_PWD2, buf2, sizeof(buf2));
			diff = (strcmp(buf1, buf2) != 0);
			memset(buf1, 0, sizeof(buf1));
			memset(buf2, 0, sizeof(buf2));
			if (diff) {
				MessageBox(hWnd, "Passwords don't match", "EncFS", MB_ICONERROR);
				return TRUE;
			}
			i = SendDlgItemMessage(hWnd, IDC_CMBDRIVE, CB_GETCURSEL, 0, 0);
			if (i != CB_ERR) {
				buf1[0] = 0;
				SendDlgItemMessage(hWnd, IDC_CMBDRIVE, CB_GETLBTEXT, i, (LPARAM) (LPTSTR) buf1);
				pData->drive = buf1[0];
			}
			GetDlgItemText(hWnd, IDC_PWD1, pData->password, sizeof(pData->password));
			EndDialog(hWnd, LOWORD(wParam));
			return TRUE;
		}
		break;
	case WM_SYSCOMMAND:
		if ((wParam & 0xFFF0) == SC_MINIMIZE)
			return 1;
		break;
	case WM_CLOSE:
		EndDialog(hWnd, IDCANCEL);
		return TRUE;
	}
	return 0;
}

static INT_PTR CALLBACK
MainDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int id;

	try {

	switch (message) {
	case WM_INITDIALOG:
		Drives::Load();
		return TRUE;
	case SWM_TRAYMSG:
		switch (lParam) {
		case WM_LBUTTONDBLCLK:
			OpenOrCreate(hWnd);
			break;
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_CONTEXTMENU:
			ShowContextMenu(hWnd);
		}
		break;
	case WM_COMMAND:
		id = LOWORD(wParam);
		switch (id) {
		case IDM_OPEN:
			OpenOrCreate(hWnd);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case IDM_ABOUT:
			DialogBox(hInst, (LPCTSTR) IDD_ABOUTBOX, hWnd, (DLGPROC) AboutDlgProc);
			break;
		default:
			if (id >= IDM_MOUNT_START && id < IDM_MOUNT_END) {
				boost::shared_ptr<Drive> drive = Drives::GetDrive(IDM_MOUNT_N(id));
				if (!drive)
					return 1;

				switch (IDM_MOUNT_TYPE(id)) {
				case IDM_TYPE_MOUNT:
					drive->Mount(hWnd);
					break;
				case IDM_TYPE_UMOUNT:
					drive->Umount(hWnd);
					break;
				case IDM_TYPE_SHOW:
					drive->Show(hWnd);
					break;
				}
			}
			break;
		}
		return 1;
	case WM_DESTROY:
		niData.uFlags = 0;
		Shell_NotifyIcon(NIM_DELETE, &niData);
		PostQuitMessage(0);
		break;
	}
	return 0;
	}
	catch (const std::runtime_error &err) {
		MessageBox(hWnd, err.what(), "EncFS", MB_ICONERROR);
		return 1;
	}
}

static INT_PTR CALLBACK
AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM /* lParam*/)
{
	switch (message) {
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

