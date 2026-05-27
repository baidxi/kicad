/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file test_plm_navigation_conformance.cpp
 * @brief Navigation conformance tests for PLM plugins.
 *
 * These tests verify that PLM plugin navigation behavior
 * conforms to the expected interface contract.
 */

#include <boost/test/unit_test.hpp>
#include "plm_conformance_framework.h"


BOOST_AUTO_TEST_SUITE( PlmNavigationConformance )


BOOST_AUTO_TEST_CASE( FolderStructConstruction )
{
    PLM_FOLDER folder;
    BOOST_CHECK( folder.id.IsEmpty() );
    BOOST_CHECK( folder.name.IsEmpty() );
    BOOST_CHECK( folder.parent_id.IsEmpty() );
    BOOST_CHECK( !folder.has_children );

    PLM_FOLDER folder2( wxS( "folder-1" ), wxS( "Projects" ) );
    BOOST_CHECK( folder2.id == wxS( "folder-1" ) );
    BOOST_CHECK( folder2.name == wxS( "Projects" ) );
    BOOST_CHECK( folder2.parent_id.IsEmpty() );
    BOOST_CHECK( folder2.IsRoot() );

    PLM_FOLDER folder3( wxS( "folder-2" ), wxS( "Subprojects" ), wxS( "folder-1" ), true );
    BOOST_CHECK( folder3.id == wxS( "folder-2" ) );
    BOOST_CHECK( folder3.parent_id == wxS( "folder-1" ) );
    BOOST_CHECK( !folder3.IsRoot() );
    BOOST_CHECK( folder3.has_children );
}


BOOST_AUTO_TEST_CASE( FolderIsRoot )
{
    PLM_FOLDER rootFolder( wxS( "root" ), wxS( "Root" ) );
    BOOST_CHECK( rootFolder.IsRoot() );

    PLM_FOLDER childFolder( wxS( "child" ), wxS( "Child" ), wxS( "root" ) );
    BOOST_CHECK( !childFolder.IsRoot() );
}


BOOST_AUTO_TEST_CASE( ListOptionsDefaults )
{
    PLM_LIST_OPTIONS opts;
    BOOST_CHECK( opts.offset == 0 );
    BOOST_CHECK( opts.limit == 50 );
    BOOST_CHECK( opts.sort_field.IsEmpty() );
    BOOST_CHECK( opts.sort_ascending );
}


BOOST_AUTO_TEST_CASE( ListOptionsFirstPage )
{
    PLM_LIST_OPTIONS opts = PLM_LIST_OPTIONS::FirstPage( 25 );
    BOOST_CHECK( opts.offset == 0 );
    BOOST_CHECK( opts.limit == 25 );
}


BOOST_AUTO_TEST_CASE( ListOptionsNextPage )
{
    PLM_LIST_OPTIONS opts( 0, 20 );
    PLM_LIST_OPTIONS next = opts.NextPage( 20 );

    BOOST_CHECK( next.offset == 20 );
    BOOST_CHECK( next.limit == 20 );

    PLM_LIST_OPTIONS next2 = next.NextPage( 20 );
    BOOST_CHECK( next2.offset == 40 );
}


BOOST_AUTO_TEST_CASE( ListOptionsPreviousPage )
{
    PLM_LIST_OPTIONS opts( 40, 20 );
    PLM_LIST_OPTIONS prev = opts.PreviousPage();

    BOOST_CHECK( prev.offset == 20 );
    BOOST_CHECK( prev.limit == 20 );

    PLM_LIST_OPTIONS prev2 = prev.PreviousPage();
    BOOST_CHECK( prev2.offset == 0 );

    PLM_LIST_OPTIONS prev3 = prev2.PreviousPage();
    BOOST_CHECK( prev3.offset == 0 );
}


BOOST_AUTO_TEST_CASE( ListResultEmpty )
{
    PLM_LIST_RESULT<PLM_FOLDER> result;
    BOOST_CHECK( result.IsEmpty() );
    BOOST_CHECK( result.Count() == 0 );
    BOOST_CHECK( result.total_count == 0 );
    BOOST_CHECK( !result.HasNextPage() );
    BOOST_CHECK( !result.HasPreviousPage() );
}


BOOST_AUTO_TEST_CASE( ListResultPagination )
{
    std::vector<PLM_FOLDER> items;

    for( int i = 0; i < 10; i++ )
        items.push_back( PLM_FOLDER( wxString::Format( wxS( "f%d" ), i ), wxString::Format( wxS( "Folder %d" ), i ) ) );

    PLM_LIST_RESULT<PLM_FOLDER> result( items, 50, 0, 10 );

    BOOST_CHECK( !result.IsEmpty() );
    BOOST_CHECK( result.Count() == 10 );
    BOOST_CHECK( result.total_count == 50 );
    BOOST_CHECK( result.HasNextPage() );
    BOOST_CHECK( !result.HasPreviousPage() );
    BOOST_CHECK( result.CurrentPage() == 1 );
    BOOST_CHECK( result.TotalPages() == 5 );
}


BOOST_AUTO_TEST_CASE( ListResultMiddlePage )
{
    std::vector<PLM_FOLDER> items;

    for( int i = 0; i < 10; i++ )
        items.push_back( PLM_FOLDER( wxString::Format( wxS( "f%d" ), i ), wxString::Format( wxS( "Folder %d" ), i ) ) );

    PLM_LIST_RESULT<PLM_FOLDER> result( items, 50, 20, 10 );

    BOOST_CHECK( result.HasNextPage() );
    BOOST_CHECK( result.HasPreviousPage() );
    BOOST_CHECK( result.CurrentPage() == 3 );
}


BOOST_AUTO_TEST_CASE( ListResultLastPage )
{
    std::vector<PLM_FOLDER> items;

    for( int i = 0; i < 5; i++ )
        items.push_back( PLM_FOLDER( wxString::Format( wxS( "f%d" ), i ), wxString::Format( wxS( "Folder %d" ), i ) ) );

    PLM_LIST_RESULT<PLM_FOLDER> result( items, 45, 40, 10 );

    BOOST_CHECK( !result.HasNextPage() );
    BOOST_CHECK( result.HasPreviousPage() );
    BOOST_CHECK( result.CurrentPage() == 5 );
    BOOST_CHECK( result.TotalPages() == 5 );
}


BOOST_AUTO_TEST_CASE( PathElementConstruction )
{
    PLM_PATH_ELEMENT elem;
    BOOST_CHECK( elem.id.IsEmpty() );
    BOOST_CHECK( elem.name.IsEmpty() );

    PLM_PATH_ELEMENT elem2( wxS( "folder-1" ), wxS( "Root" ) );
    BOOST_CHECK( elem2.id == wxS( "folder-1" ) );
    BOOST_CHECK( elem2.name == wxS( "Root" ) );
}


BOOST_AUTO_TEST_CASE( FolderPathConstruction )
{
    PLM_FOLDER_PATH path;
    BOOST_CHECK( path.empty() );

    path.push_back( PLM_PATH_ELEMENT( wxS( "root" ), wxS( "Root" ) ) );
    path.push_back( PLM_PATH_ELEMENT( wxS( "projects" ), wxS( "Projects" ) ) );
    path.push_back( PLM_PATH_ELEMENT( wxS( "myproj" ), wxS( "My Project" ) ) );

    BOOST_CHECK( path.size() == 3 );
    BOOST_CHECK( path[0].name == wxS( "Root" ) );
    BOOST_CHECK( path[2].name == wxS( "My Project" ) );
}


BOOST_AUTO_TEST_CASE( SearchFieldText )
{
    PLM_SEARCH_FIELD field = PLM_SEARCH_FIELD::Text( wxS( "name" ), wxS( "Name" ) );

    BOOST_CHECK( field.field_id == wxS( "name" ) );
    BOOST_CHECK( field.display_name == wxS( "Name" ) );
    BOOST_CHECK( field.field_type == PLM_SEARCH_FIELD_TYPE::TEXT );
    BOOST_CHECK( !field.required );
}


BOOST_AUTO_TEST_CASE( SearchFieldDropdown )
{
    std::vector<wxString> options = { wxS( "draft" ), wxS( "review" ), wxS( "released" ) };
    PLM_SEARCH_FIELD field = PLM_SEARCH_FIELD::Dropdown( wxS( "status" ), wxS( "Status" ), options );

    BOOST_CHECK( field.field_type == PLM_SEARCH_FIELD_TYPE::DROPDOWN );
    BOOST_CHECK( field.options.size() == 3 );
    BOOST_CHECK( field.options[0] == wxS( "draft" ) );
}


BOOST_AUTO_TEST_CASE( SearchFieldDate )
{
    PLM_SEARCH_FIELD field = PLM_SEARCH_FIELD::Date( wxS( "created" ), wxS( "Created Date" ) );

    BOOST_CHECK( field.field_type == PLM_SEARCH_FIELD_TYPE::DATE );
}


BOOST_AUTO_TEST_CASE( SearchFieldUser )
{
    PLM_SEARCH_FIELD field = PLM_SEARCH_FIELD::User( wxS( "author" ), wxS( "Author" ) );

    BOOST_CHECK( field.field_type == PLM_SEARCH_FIELD_TYPE::USER );
}


BOOST_AUTO_TEST_CASE( MockNavigationNotAuthenticated )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    BOOST_CHECK( !mock.IsAuthenticated() );

    std::vector<PLM_FOLDER> folders = mock.GetRootFolders();
    BOOST_CHECK( folders.empty() );
}


BOOST_AUTO_TEST_CASE( MockNavigationAuthenticated )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    std::vector<PLM_FOLDER> testFolders;
    testFolders.push_back( PLM_FOLDER( wxS( "f1" ), wxS( "Folder 1" ) ) );
    testFolders.push_back( PLM_FOLDER( wxS( "f2" ), wxS( "Folder 2" ) ) );
    testFolders.push_back( PLM_FOLDER( wxS( "f3" ), wxS( "Folder 3" ) ) );
    mock.SetRootFolders( testFolders );

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://plm.example.com" );
    creds.username = wxS( "testuser" );
    mock.Authenticate( creds );

    std::vector<PLM_FOLDER> folders = mock.GetRootFolders();
    BOOST_CHECK( folders.size() == 3 );
    BOOST_CHECK( folders[0].id == wxS( "f1" ) );
    BOOST_CHECK( folders[0].name == wxS( "Folder 1" ) );
}


BOOST_AUTO_TEST_CASE( MockNavigationChildFolders )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    std::vector<PLM_FOLDER> children = mock.GetChildFolders( wxS( "folder-1" ) );
    BOOST_CHECK( children.empty() );
}


BOOST_AUTO_TEST_CASE( MockNavigationFolderPath )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_FOLDER_PATH path = mock.GetFolderPath( wxS( "folder-1" ) );
    BOOST_CHECK( path.empty() );
}


BOOST_AUTO_TEST_CASE( MockNavigationEmptyFolders )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_CREDENTIALS creds;
    creds.server_url = wxS( "https://plm.example.com" );
    creds.username = wxS( "testuser" );
    mock.Authenticate( creds );

    std::vector<PLM_FOLDER> folders = mock.GetRootFolders();
    BOOST_CHECK( folders.empty() );
}


BOOST_AUTO_TEST_CASE( MockFileListEmpty )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_LIST_OPTIONS opts;
    PLM_LIST_RESULT<PLM_FILE_INFO> result = mock.ListFiles( wxS( "folder-1" ), opts );

    BOOST_CHECK( result.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( MockSearchEmpty )
{
    PLM_MOCK_PLUGIN mock;
    mock.OnLoad();

    PLM_LIST_OPTIONS opts;
    PLM_LIST_RESULT<PLM_FILE_INFO> result = mock.SearchFiles( wxS( "test query" ), opts );

    BOOST_CHECK( result.IsEmpty() );
}


BOOST_AUTO_TEST_SUITE_END()
