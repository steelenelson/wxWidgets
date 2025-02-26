/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/filesys.cpp
// Purpose:     wxFileSystem class - interface for opening files
// Author:      Vaclav Slavik
// Copyright:   (c) 1999 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"



#if wxUSE_FILESYSTEM

#include "wx/filesys.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/module.h"
#endif

#include "wx/sysopt.h"
#include "wx/wfstream.h"
#include "wx/mimetype.h"
#include "wx/filename.h"
#include "wx/tokenzr.h"
#include "wx/private/fileback.h"
#include "wx/utils.h"

// ----------------------------------------------------------------------------
// wxFSFile
// ----------------------------------------------------------------------------

const wxString& wxFSFile::GetMimeType() const
{
    if ( m_MimeType.empty() && !m_Location.empty() )
    {
        wxConstCast(this, wxFSFile)->m_MimeType =
            wxFileSystemHandler::GetMimeTypeFromExt(m_Location);
    }

    return m_MimeType;
}

// ----------------------------------------------------------------------------
// wxFileSystemHandler
// ----------------------------------------------------------------------------

wxIMPLEMENT_ABSTRACT_CLASS(wxFileSystemHandler, wxObject);


/* static */
wxString wxFileSystemHandler::GetMimeTypeFromExt(const wxString& location)
{
    wxString ext, mime;
    wxString loc = GetRightLocation(location);
    int l = loc.length(), l2;

    l2 = l;
    for (int i = l-1; i >= 0; i--)
    {
        wxChar c;
        c = loc[(size_t) i];
        if ( c == wxT('#') )
            l2 = i + 1;
        if ( c == wxT('.') )
        {
            ext = loc.Right(l2-i-1);
            break;
        }
        if ( (c == wxT('/')) || (c == wxT('\\')) || (c == wxT(':')) )
            return wxEmptyString;
    }

#if wxUSE_MIMETYPE
    static bool s_MinimalMimeEnsured = false;

    // Don't use mime types manager if the application doesn't need it and it would be
    // cause an unacceptable delay, especially on startup.
#if wxUSE_SYSTEM_OPTIONS
    if ( !wxSystemOptions::GetOptionInt(wxT("filesys.no-mimetypesmanager")) )
#endif
    {
        if (!s_MinimalMimeEnsured)
        {
            static const wxFileTypeInfo fallbacks[] =
            {
                wxFileTypeInfo(wxT("image/jpeg"),
                    wxEmptyString,
                    wxEmptyString,
                    wxT("JPEG image (from fallback)"),
                    wxT("jpg"), wxT("jpeg"), wxT("JPG"), wxT("JPEG"), wxNullPtr),
                wxFileTypeInfo(wxT("image/gif"),
                    wxEmptyString,
                    wxEmptyString,
                    wxT("GIF image (from fallback)"),
                    wxT("gif"), wxT("GIF"), wxNullPtr),
                wxFileTypeInfo(wxT("image/png"),
                    wxEmptyString,
                    wxEmptyString,
                    wxT("PNG image (from fallback)"),
                    wxT("png"), wxT("PNG"), wxNullPtr),
                wxFileTypeInfo(wxT("image/bmp"),
                    wxEmptyString,
                    wxEmptyString,
                    wxT("windows bitmap image (from fallback)"),
                    wxT("bmp"), wxT("BMP"), wxNullPtr),
                wxFileTypeInfo(wxT("text/html"),
                    wxEmptyString,
                    wxEmptyString,
                    wxT("HTML document (from fallback)"),
                    wxT("htm"), wxT("html"), wxT("HTM"), wxT("HTML"), wxNullPtr),
                // must terminate the table with this!
                wxFileTypeInfo()
            };
            wxTheMimeTypesManager->AddFallbacks(fallbacks);
            s_MinimalMimeEnsured = true;
        }

        wxFileType *ft = wxTheMimeTypesManager->GetFileTypeFromExtension(ext);
        if ( !ft || !ft -> GetMimeType(&mime) )
        {
            mime.clear();
        }

        delete ft;

        return mime;
    }
#endif // wxUSE_MIMETYPE

    // Without wxUSE_MIMETYPE, recognize just a few hardcoded special cases.
    if ( ext.IsSameAs(wxT("htm"), false) || ext.IsSameAs(wxT("html"), false) )
        return wxT("text/html");
    if ( ext.IsSameAs(wxT("jpg"), false) || ext.IsSameAs(wxT("jpeg"), false) )
        return wxT("image/jpeg");
    if ( ext.IsSameAs(wxT("gif"), false) )
        return wxT("image/gif");
    if ( ext.IsSameAs(wxT("png"), false) )
        return wxT("image/png");
    if ( ext.IsSameAs(wxT("bmp"), false) )
        return wxT("image/bmp");

    return wxString();
}



/* static */
wxString wxFileSystemHandler::GetProtocol(const wxString& location)
{
    wxString s;
    int i, l = location.length();
    bool fnd = false;

    for (i = l-1; (i >= 0) && ((location[i] != wxT('#')) || (!fnd)); i--) {
        if ((location[i] == wxT(':')) && (i != 1 /*win: C:\path*/)) fnd = true;
    }
    if (!fnd) return wxT("file");
    for (++i; (i < l) && (location[i] != wxT(':')); i++) s << location[i];
    return s;
}


/* static */
wxString wxFileSystemHandler::GetLeftLocation(const wxString& location)
{
    int i;
    bool fnd = false;

    for (i = location.length()-1; i >= 0; i--) {
        if ((location[i] == wxT(':')) && (i != 1 /*win: C:\path*/)) fnd = true;
        else if (fnd && (location[i] == wxT('#'))) return location.Left(i);
    }
    return wxEmptyString;
}

/* static */
wxString wxFileSystemHandler::GetRightLocation(const wxString& location)
{
    int i, len = location.length();
    for (i = len-1; i >= 0; i--)
    {
        if (location[i] == wxT('#'))
            len = i;
        if (location[i] != wxT(':'))
            continue;

        // C: on Windows
        if (i == 1)
            continue;
        if (i >= 2 && wxIsalpha(location[i-1]) && location[i-2] == wxT('/'))
            continue;

        // Could be the protocol
        break;
    }
    if (i == 0) return wxEmptyString;

    const static wxString protocol(wxT("file:"));
    if (i < (int)protocol.length() - 1 || location.compare(0, i + 1, protocol))
        return location.Mid(i + 1, len - i - 1);

    int s = ++i; // Start position
    // Check if there are three '/'s after "file:"
    int end = wxMin(len, s + 3);
    while (i < end && location[i] == wxT('/'))
        i++;
    if (i == s + 2) // Host is specified, e.g. "file://host/path"
        return location.Mid(s, len - s);
    if (i > s)
    {
        // Remove the last '/' if it is preceding "C:/...".
        // Otherwise, keep it.
        if (i + 1 >= len || location[i + 1] != wxT(':'))
            i--;
        else if (i + 4 < len)
        {
            // Check if ':' was encoded
            const static wxString colonLower(wxT("%3a"));
            const static wxString colonUpper(wxT("%3A"));
            wxString sub =location.Mid(i + 1, 3);
            if (sub == colonLower || sub == colonUpper)
                i--;
        }
    }
    return location.Mid(i, len - i);
}

/* static */
wxString wxFileSystemHandler::GetAnchor(const wxString& location)
{
    int l = location.length();

    for (int i = l-1; i >= 0; i--) {
        wxChar c;
        c = location[i];
        if (c == wxT('#'))
            return location.Right(l-i-1);
        else if ((c == wxT('/')) || (c == wxT('\\')) || (c == wxT(':')))
            break;
    }
    return wxEmptyString;
}


wxString wxFileSystemHandler::FindFirst(const wxString& WXUNUSED(spec),
                                        int WXUNUSED(flags))
{
    return wxEmptyString;
}

wxString wxFileSystemHandler::FindNext()
{
    return wxEmptyString;
}

//--------------------------------------------------------------------------------
// wxLocalFSHandler
//--------------------------------------------------------------------------------


wxString wxLocalFSHandler::ms_root;

bool wxLocalFSHandler::CanOpen(const wxString& location)
{
    return GetProtocol(location) == wxT("file");
}

wxFSFile* wxLocalFSHandler::OpenFile(wxFileSystem& WXUNUSED(fs), const wxString& location)
{
    // location has Unix path separators
    wxString right = GetRightLocation(location);
    wxFileName fn = wxFileName::URLToFileName(right);
    wxString fullpath = ms_root + fn.GetFullPath();

    if (!wxFileExists(fullpath))
        return NULL;

    // we need to check whether we can really read from this file, otherwise
    // wxFSFile is not going to work
#if wxUSE_FFILE
    wxFFileInputStream *is = new wxFFileInputStream(fullpath);
#elif wxUSE_FILE
    wxFileInputStream *is = new wxFileInputStream(fullpath);
#else
#error One of wxUSE_FILE or wxUSE_FFILE must be set to 1 for wxFSHandler to work
#endif
    if ( !is->IsOk() )
    {
        delete is;
        return NULL;
    }

    return new wxFSFile(is,
                        location,
                        wxEmptyString,
                        GetAnchor(location)
#if wxUSE_DATETIME
                        ,wxDateTime(wxFileModificationTime(fullpath))
#endif // wxUSE_DATETIME
                        );
}

wxString wxLocalFSHandler::FindFirst(const wxString& spec, int flags)
{
    wxFileName fn = wxFileName::URLToFileName(GetRightLocation(spec));
    const wxString found = wxFindFirstFile(ms_root + fn.GetFullPath(), flags);
    if ( found.empty() )
        return found;
    return wxFileSystem::FileNameToURL(found);
}

wxString wxLocalFSHandler::FindNext()
{
    const wxString found = wxFindNextFile();
    if ( found.empty() )
        return found;
    return wxFileSystem::FileNameToURL(found);
}



//-----------------------------------------------------------------------------
// wxFileSystem
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxFileSystem, wxObject);
wxIMPLEMENT_ABSTRACT_CLASS(wxFSFile, wxObject);


wxList wxFileSystem::m_Handlers;


wxFileSystem::~wxFileSystem()
{
    WX_CLEAR_HASH_MAP(wxFSHandlerHash, m_LocalHandlers)
}


static wxString MakeCorrectPath(const wxString& path)
{
    wxString p(path);
    wxString r;
    int i, j, cnt;

    cnt = p.length();
    for (i = 0; i < cnt; i++)
      if (p.GetChar(i) == wxT('\\')) p.GetWritableChar(i) = wxT('/'); // Want to be windows-safe

    if (p.Left(2) == wxT("./")) { p = p.Mid(2); cnt -= 2; }

    if (cnt < 3) return p;

    r << p.GetChar(0) << p.GetChar(1);

    // skip trailing ../.., if any
    for (i = 2; i < cnt && (p.GetChar(i) == wxT('/') || p.GetChar(i) == wxT('.')); i++) r << p.GetChar(i);

    // remove back references: translate dir1/../dir2 to dir2
    for (; i < cnt; i++)
    {
        r << p.GetChar(i);
        if (p.GetChar(i) == wxT('/') && p.GetChar(i-1) == wxT('.') && p.GetChar(i-2) == wxT('.'))
        {
            for (j = r.length() - 2; j >= 0 && r.GetChar(j) != wxT('/') && r.GetChar(j) != wxT(':'); j--) {}
            if (j >= 0 && r.GetChar(j) != wxT(':'))
            {
                for (j = j - 1; j >= 0 && r.GetChar(j) != wxT('/') && r.GetChar(j) != wxT(':'); j--) {}
                r.Remove(j + 1);
            }
        }
    }

    for (; i < cnt; i++) r << p.GetChar(i);

    return r;
}


void wxFileSystem::ChangePathTo(const wxString& location, bool is_dir)
{

    m_Path = MakeCorrectPath(location);

    if (is_dir)
    {
        if (!m_Path.empty() && m_Path.Last() != wxT('/') && m_Path.Last() != wxT(':'))
            m_Path << wxT('/');
    }
    else
    {
        int i, pathpos = -1;
        for (i = m_Path.length()-1; i >= 0; i--)
        {
            if (m_Path[(size_t) i] == wxT('/'))
            {
                if ((i > 1) && (m_Path[(size_t) (i-1)] == wxT('/')) && (m_Path[(size_t) (i-2)] == wxT(':')))
                {
                    i -= 2;
                    continue;
                }
                else
                {
                    pathpos = i;
                    break;
                }
            }
            else if (m_Path[(size_t) i] == wxT(':')) {
                pathpos = i;
                break;
            }
        }
        if (pathpos == -1)
        {
            for (i = 0; i < (int) m_Path.length(); i++)
            {
                if (m_Path[(size_t) i] == wxT(':'))
                {
                    m_Path.Remove(i+1);
                    break;
                }
            }
            if (i == (int) m_Path.length())
                m_Path.clear();
        }
        else
        {
            m_Path.Remove(pathpos+1);
        }
    }
}



wxFileSystemHandler *wxFileSystem::MakeLocal(wxFileSystemHandler *h)
{
    wxClassInfo *classinfo = h->GetClassInfo();

    if (classinfo->IsDynamic())
    {
        wxFileSystemHandler*& local = m_LocalHandlers[classinfo];
        if (!local)
            local = (wxFileSystemHandler*)classinfo->CreateObject();
        return local;
    }
    else
    {
        return h;
    }
}



wxFSFile* wxFileSystem::OpenFile(const wxString& location, int flags)
{
    if ((flags & wxFS_READ) == 0)
        return NULL;

    wxString loc = MakeCorrectPath(location);
    unsigned i, ln;
    wxChar meta;
    wxFSFile *s = NULL;
    wxList::compatibility_iterator node;

    ln = loc.length();
    meta = 0;
    for (i = 0; i < ln; i++)
    {
        switch ( loc[(size_t)i].GetValue() )
        {
            case wxT('/') : case wxT(':') : case wxT('#') :
                meta = loc[(size_t)i];
                break;
        }
        if (meta != 0) break;
    }
    m_LastName.clear();

    // try relative paths first :
    if (meta != wxT(':') && !m_Path.empty())
    {
        const wxString fullloc = m_Path + loc;
        node = m_Handlers.GetFirst();
        while (node)
        {
            wxFileSystemHandler *h = (wxFileSystemHandler*) node -> GetData();
            if (h->CanOpen(fullloc))
            {
                s = MakeLocal(h)->OpenFile(*this, fullloc);
                if (s) { m_LastName = fullloc; break; }
            }
            node = node->GetNext();
        }
    }

    // if failed, try absolute paths :
    if (s == NULL)
    {
        node = m_Handlers.GetFirst();
        while (node)
        {
            wxFileSystemHandler *h = (wxFileSystemHandler*) node->GetData();
            if (h->CanOpen(loc))
            {
                s = MakeLocal(h)->OpenFile(*this, loc);
                if (s) { m_LastName = loc; break; }
            }
            node = node->GetNext();
        }
    }

    if (s && (flags & wxFS_SEEKABLE) != 0 && !s->GetStream()->IsSeekable())
    {
        wxBackedInputStream *stream;
        stream = new wxBackedInputStream(s->DetachStream());
        stream->FindLength();
        s->SetStream(stream);
    }

    return (s);
}



wxString wxFileSystem::FindFirst(const wxString& spec, int flags)
{
    wxList::compatibility_iterator node;
    wxString spec2(spec);

    m_FindFileHandler = NULL;

    for (int i = spec2.length()-1; i >= 0; i--)
        if (spec2[(size_t) i] == wxT('\\')) spec2.GetWritableChar(i) = wxT('/'); // Want to be windows-safe

    node = m_Handlers.GetFirst();
    while (node)
    {
        wxFileSystemHandler *h = (wxFileSystemHandler*) node -> GetData();
        if (h -> CanOpen(m_Path + spec2))
        {
            m_FindFileHandler = MakeLocal(h);
            return m_FindFileHandler -> FindFirst(m_Path + spec2, flags);
        }
        node = node->GetNext();
    }

    node = m_Handlers.GetFirst();
    while (node)
    {
        wxFileSystemHandler *h = (wxFileSystemHandler*) node -> GetData();
        if (h -> CanOpen(spec2))
        {
            m_FindFileHandler = MakeLocal(h);
            return m_FindFileHandler -> FindFirst(spec2, flags);
        }
        node = node->GetNext();
    }

    return wxEmptyString;
}



wxString wxFileSystem::FindNext()
{
    if (m_FindFileHandler == NULL) return wxEmptyString;
    else return m_FindFileHandler -> FindNext();
}

bool wxFileSystem::FindFileInPath(wxString *pStr,
                                  const wxString& path,
                                  const wxString& basename)
{
    // we assume that it's not empty
    wxCHECK_MSG( !basename.empty(), false,
                wxT("empty file name in wxFileSystem::FindFileInPath"));

    wxString name;
    // skip path separator in the beginning of the file name if present
    if ( wxIsPathSeparator(basename[(size_t)0u]) )
        name = basename.substr(1);
    else
        name = basename;

    wxStringTokenizer tokenizer(path, wxPATH_SEP);
    while ( tokenizer.HasMoreTokens() )
    {
        wxString strFile = tokenizer.GetNextToken();
        if ( !wxEndsWithPathSeparator(strFile) )
            strFile += wxFILE_SEP_PATH;
        strFile += name;

        wxFSFile *file = OpenFile(strFile);
        if ( file )
        {
            delete file;
            *pStr = strFile;
            return true;
        }
    }

    return false;
}

void wxFileSystem::AddHandler(wxFileSystemHandler *handler)
{
    // prepend the handler to the beginning of the list because handlers added
    // last should have the highest priority to allow overriding them
    m_Handlers.Insert((size_t)0, handler);
}

wxFileSystemHandler* wxFileSystem::RemoveHandler(wxFileSystemHandler *handler)
{
    // if handler has already been removed (or deleted)
    // we return NULL. This is by design in case
    // CleanUpHandlers() is called before RemoveHandler
    // is called, as we cannot control the order
    // which modules are unloaded
    if (!m_Handlers.DeleteObject(handler))
        return NULL;

    return handler;
}


bool wxFileSystem::HasHandlerForPath(const wxString &location)
{
    for ( wxList::compatibility_iterator node = m_Handlers.GetFirst();
           node; node = node->GetNext() )
    {
        wxFileSystemHandler *h = (wxFileSystemHandler*) node->GetData();
        if (h->CanOpen(location))
            return true;
    }

    return false;
}

void wxFileSystem::CleanUpHandlers()
{
    WX_CLEAR_LIST(wxList, m_Handlers);
}

// Returns the native path for a file URL
wxFileName wxFileSystem::URLToFileName(const wxString& url)
{
    return wxFileName::URLToFileName( url );
}

// Returns the file URL for a native path
wxString wxFileSystem::FileNameToURL(const wxFileName& filename)
{
    return wxFileName::FileNameToURL( filename );
}


///// Module:

class wxFileSystemModule : public wxModule
{
    wxDECLARE_DYNAMIC_CLASS(wxFileSystemModule);

    public:
        wxFileSystemModule() :
            wxModule(),
            m_handler(NULL)
        {
        }

        virtual bool OnInit() wxOVERRIDE
        {
            m_handler = new wxLocalFSHandler;
            wxFileSystem::AddHandler(m_handler);
            return true;
        }
        virtual void OnExit() wxOVERRIDE
        {
            delete wxFileSystem::RemoveHandler(m_handler);

            wxFileSystem::CleanUpHandlers();
        }

    private:
        wxFileSystemHandler* m_handler;

};

wxIMPLEMENT_DYNAMIC_CLASS(wxFileSystemModule, wxModule);

//// wxFSInputStream

wxFSInputStream::wxFSInputStream(const wxString& filename, int flags)
{
    wxFileSystem fs;
    m_file = fs.OpenFile(filename, flags | wxFS_READ);

    if ( m_file )
    {
        wxInputStream* const stream = m_file->GetStream();
        if ( stream )
        {
            // Notice that we pass the stream by reference: it shouldn't be
            // deleted by us as it's owned by m_file already.
            InitParentStream(*stream);
        }
    }
}

wxFSInputStream::~wxFSInputStream()
{
    delete m_file;
}

#endif
  // wxUSE_FILESYSTEM
