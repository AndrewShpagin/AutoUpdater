#include <process.h>
#ifdef _WIN32

#include <windows.h>
#include <shlobj.h>
#include <cassert>
#include <codecvt>
#include <string>

// convert UTF-8 string to wstring
std::wstring utf8_to_wstring(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(str);
}

/*============================================================================*/

// CreateLink - Uses the Shell's IShellLink and IPersistFile interfaces 
//              to create and store a shortcut to the specified object. 
//
// Returns the result of calling the member functions of the interfaces. 
//
// Parameters:
// lpszPathObj  - Address of a buffer that contains the path of the object,
//                including the file name.
// lpszPathLink - Address of a buffer that contains the path where the 
//                Shell link is to be stored, including the file name.
// lpszPath     - Working directory of target Obj file
// lpszDesc     - Address of a buffer that contains a description of the 
//                Shell link, stored in the Comment field of the link
//                properties.

HRESULT                     CreateLink(
    LPCSTR                      lpszPathObj,
    LPCSTR                      lpszPathLink,
    LPCSTR                      lpszPath,
    LPCSTR                      lpszDesc)

    /*============================================================================*/
{
    std::wstring PathObj = utf8_to_wstring(lpszPathObj);
    std::wstring PathLink = utf8_to_wstring(lpszPathLink);
    std::wstring Path = utf8_to_wstring(lpszPath);
    std::wstring Desc = utf8_to_wstring(lpszDesc);

    IShellLinkW* psl = NULL;
    HRESULT hres = CoInitialize(NULL);

    if (!SUCCEEDED(hres))
        return 0;

    // Get a pointer to the IShellLink interface. It is assumed that CoInitialize
    // has already been called.

    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl);
    if (SUCCEEDED(hres))
    {
        IPersistFile* ppf;

        // Set the path to the shortcut target and add the description. 
        psl->SetPath(PathObj.c_str());
        psl->SetDescription(Desc.c_str());
        psl->SetWorkingDirectory(Path.c_str());

        // Query IShellLink for the IPersistFile interface, used for saving the 
        // shortcut in persistent storage. 
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

        if (SUCCEEDED(hres))
        {

            // Save the link by calling IPersistFile::Save. 
            hres = ppf->Save(PathLink.c_str(), TRUE);
            if (!SUCCEEDED(hres))
                assert(FALSE);

            ppf->Release();
        }
        psl->Release();
    }

    CoUninitialize();

    return hres;
}

void shortcut(const char* exepath, const char* menupath) {

    WCHAR* path;
    SHGetKnownFolderPath(FOLDERID_Programs, 0, NULL, &path);
    char pth[2048];
    wcstombs(pth, path, 2048);
    std::string sShortcutPath = std::string(pth) + "\\" + menupath;
    CreateLink(exepath, sShortcutPath.c_str(), "", "The updater");
}

void execute(const std::string& exepath, const std::string& params) {
    ShellExecute(0, "open", exepath.c_str(), params.c_str(),"",SW_SHOW);
}

#else
void shortcut(const char* exepath, const char* menupath) {
	
}

void execute(const std::string& exepath, const std::string& params) {
    execl(exepath.c_str(), params.c_str(), NULL);
}

#endif
