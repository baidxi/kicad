#ifndef ALLEGRO_PLUGIN_H_
#define ALLEGRO_PLUGIN_H_

#include <wx/log.h>
#include <pcb_io/pcb_io.h>
#include <pcb_io/common/plugin_common_layer_mapping.h>

class PCB_IO_ALLEGRO : public PCB_IO, public LAYER_MAPPABLE_PLUGIN
{
public:
    const IO_BASE::IO_FILE_DESC GetBoardFileDesc() const override
    {
        return IO_BASE::IO_FILE_DESC( _HKI( "Cadence Allegro layout file" ), { "brd" } );
    }

    const IO_BASE::IO_FILE_DESC GetLibraryDesc() const override { return GetBoardFileDesc(); }

    const IO_BASE::IO_FILE_DESC GetFootprintFileDesc() const
    {
        wxLogWarning( "Func %s", __FUNCTION__ );
        return GetBoardFileDesc();
    }

    const IO_BASE::IO_FILE_DESC GetFootprintLibDesc() const
    {
        wxLogWarning( "Func? %s", __FUNCTION__ );
        return GetBoardFileDesc();
    }

    long long GetLibraryTimestamp( const wxString& aLibraryPath ) const override
    {
        wxLogWarning( "Func %s", __FUNCTION__ );
        // No support for libraries....
        return 1;
    }

    bool CanReadBoard( const wxString& aFileName ) const override;
    bool CanReadFootprint( const wxString& aFileName ) const override;
    bool CanReadFootprintLib( const wxString& aFileName ) const;

    // FIXME: Should we return true if we can read all the library elements of
    // this file?
    bool CanReadLibrary( const wxString& aFileName ) const override { return false; }

    std::vector<FOOTPRINT*> GetImportedCachedLibraryFootprints() override
    {
        wxLogWarning( "Func %s", __FUNCTION__ );
        std::vector<FOOTPRINT*> fps;
        return fps;
    }

    BOARD* LoadBoard( const wxString& aFileName, BOARD* aAppendToMe,
                      const std::map<std::string, UTF8>* aProperties = nullptr,
                      PROJECT*                           aProject = nullptr ) override;

    void FootprintEnumerate( wxArrayString& aFootprintNames, const wxString& aLibraryPath,
                             bool                               aBestEfforts,
                             const std::map<std::string, UTF8>* aProperties ) override
    {
        wxLogWarning( "Func %s", __FUNCTION__ );
    }

    void FootprintLibCreate( const wxString& aLibraryPath, const STRING_UTF8_MAP* aProperties )
    {
        wxLogWarning( "Func %s", __FUNCTION__ );
    }

    bool IsFootprintLibWritable( const wxString& aLibraryPath ) { return false; }

    PCB_IO_ALLEGRO();
    ~PCB_IO_ALLEGRO() override;

private:
    BOARD* m_board;
};

#endif // ALLEGRO_PLUGIN_H_
