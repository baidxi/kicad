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
 * @file test_plm_parts.cpp
 * @brief Tests for PLM part database structures and operations.
 */

#include <boost/test/unit_test.hpp>
#include <plm/plm_parts.h>


BOOST_AUTO_TEST_SUITE( PlmParts )


/**
 * Test PLM_PART default construction.
 */
BOOST_AUTO_TEST_CASE( PartDefaultConstruction )
{
    PLM_PART part;

    BOOST_CHECK( part.part_id.IsEmpty() );
    BOOST_CHECK( part.part_number.IsEmpty() );
    BOOST_CHECK( part.manufacturer.IsEmpty() );
    BOOST_CHECK( part.description.IsEmpty() );
    BOOST_CHECK( part.state == PLM_PART_STATE::ACTIVE );
    BOOST_CHECK( part.IsAvailable() );
    BOOST_CHECK( !part.NeedsReplacement() );
    BOOST_CHECK( !part.HasReplacement() );
}


/**
 * Test PLM_PART construction with parameters.
 */
BOOST_AUTO_TEST_CASE( PartParameterizedConstruction )
{
    PLM_PART part( wxS( "part-001" ), wxS( "RC0402-10K" ), wxS( "10K Ohm 0402 Resistor" ) );

    BOOST_CHECK( part.part_id == wxS( "part-001" ) );
    BOOST_CHECK( part.part_number == wxS( "RC0402-10K" ) );
    BOOST_CHECK( part.description == wxS( "10K Ohm 0402 Resistor" ) );
    BOOST_CHECK( part.state == PLM_PART_STATE::ACTIVE );
}


/**
 * Test part availability based on state.
 */
BOOST_AUTO_TEST_CASE( PartAvailability )
{
    PLM_PART part;

    part.state = PLM_PART_STATE::ACTIVE;
    BOOST_CHECK( part.IsAvailable() );
    BOOST_CHECK( !part.NeedsReplacement() );

    part.state = PLM_PART_STATE::PREFERRED;
    BOOST_CHECK( part.IsAvailable() );
    BOOST_CHECK( !part.NeedsReplacement() );

    part.state = PLM_PART_STATE::NOT_RECOMMENDED;
    BOOST_CHECK( !part.IsAvailable() );
    BOOST_CHECK( !part.NeedsReplacement() );

    part.state = PLM_PART_STATE::OBSOLETE;
    BOOST_CHECK( !part.IsAvailable() );
    BOOST_CHECK( part.NeedsReplacement() );

    part.state = PLM_PART_STATE::END_OF_LIFE;
    BOOST_CHECK( !part.IsAvailable() );
    BOOST_CHECK( part.NeedsReplacement() );

    part.state = PLM_PART_STATE::DISCONTINUED;
    BOOST_CHECK( !part.IsAvailable() );
    BOOST_CHECK( part.NeedsReplacement() );

    part.state = PLM_PART_STATE::PENDING;
    BOOST_CHECK( !part.IsAvailable() );
    BOOST_CHECK( !part.NeedsReplacement() );

    part.state = PLM_PART_STATE::DRAFT;
    BOOST_CHECK( !part.IsAvailable() );
    BOOST_CHECK( !part.NeedsReplacement() );
}


/**
 * Test part replacement detection.
 */
BOOST_AUTO_TEST_CASE( PartReplacementDetection )
{
    PLM_PART part;

    BOOST_CHECK( !part.HasReplacement() );

    part.replacement_id = wxS( "part-002" );
    BOOST_CHECK( part.HasReplacement() );

    part.replacement_id = wxEmptyString;
    BOOST_CHECK( !part.HasReplacement() );
}


/**
 * Test part state names.
 */
BOOST_AUTO_TEST_CASE( PartStateNames )
{
    PLM_PART part;

    part.state = PLM_PART_STATE::ACTIVE;
    BOOST_CHECK( part.GetStateName() == wxS( "Active" ) );

    part.state = PLM_PART_STATE::PREFERRED;
    BOOST_CHECK( part.GetStateName() == wxS( "Preferred" ) );

    part.state = PLM_PART_STATE::NOT_RECOMMENDED;
    BOOST_CHECK( part.GetStateName() == wxS( "Not Recommended" ) );

    part.state = PLM_PART_STATE::OBSOLETE;
    BOOST_CHECK( part.GetStateName() == wxS( "Obsolete" ) );

    part.state = PLM_PART_STATE::END_OF_LIFE;
    BOOST_CHECK( part.GetStateName() == wxS( "End of Life" ) );

    part.state = PLM_PART_STATE::DISCONTINUED;
    BOOST_CHECK( part.GetStateName() == wxS( "Discontinued" ) );

    part.state = PLM_PART_STATE::PENDING;
    BOOST_CHECK( part.GetStateName() == wxS( "Pending" ) );

    part.state = PLM_PART_STATE::DRAFT;
    BOOST_CHECK( part.GetStateName() == wxS( "Draft" ) );
}


/**
 * Test PLM_PART_SEARCH_OPTIONS default construction.
 */
BOOST_AUTO_TEST_CASE( SearchOptionsDefaults )
{
    PLM_PART_SEARCH_OPTIONS opts;

    BOOST_CHECK( opts.query.IsEmpty() );
    BOOST_CHECK( opts.category_filter.IsEmpty() );
    BOOST_CHECK( opts.manufacturer_filter.IsEmpty() );
    BOOST_CHECK( opts.state_filter.empty() );
    BOOST_CHECK( !opts.include_obsolete );
    BOOST_CHECK( !opts.exact_match );
    BOOST_CHECK( opts.sort_ascending );
    BOOST_CHECK_EQUAL( opts.offset, 0 );
    BOOST_CHECK_EQUAL( opts.limit, 50 );
}


/**
 * Test PLM_PART_SEARCH_OPTIONS with query.
 */
BOOST_AUTO_TEST_CASE( SearchOptionsWithQuery )
{
    PLM_PART_SEARCH_OPTIONS opts( wxS( "10K resistor" ) );

    BOOST_CHECK( opts.query == wxS( "10K resistor" ) );
}


/**
 * Test PLM_PART_SEARCH_OPTIONS::ActiveOnly factory.
 */
BOOST_AUTO_TEST_CASE( SearchOptionsActiveOnly )
{
    auto opts = PLM_PART_SEARCH_OPTIONS::ActiveOnly( wxS( "capacitor" ) );

    BOOST_CHECK( opts.query == wxS( "capacitor" ) );
    BOOST_CHECK_EQUAL( opts.state_filter.size(), 2 );
    BOOST_CHECK( opts.state_filter[0] == PLM_PART_STATE::ACTIVE );
    BOOST_CHECK( opts.state_filter[1] == PLM_PART_STATE::PREFERRED );
}


/**
 * Test PLM_PART_SEARCH_OPTIONS::ByCategory factory.
 */
BOOST_AUTO_TEST_CASE( SearchOptionsByCategory )
{
    auto opts = PLM_PART_SEARCH_OPTIONS::ByCategory( wxS( "Resistors" ) );

    BOOST_CHECK( opts.category_filter == wxS( "Resistors" ) );
    BOOST_CHECK( opts.query.IsEmpty() );
}


/**
 * Test PLM_BOM_ISSUE construction.
 */
BOOST_AUTO_TEST_CASE( BomIssueConstruction )
{
    PLM_BOM_ISSUE issue( wxS( "R1" ), wxS( "RC0402-10K" ), wxS( "missing" ),
                          wxS( "Part not in database" ) );

    BOOST_CHECK( issue.reference == wxS( "R1" ) );
    BOOST_CHECK( issue.part_number == wxS( "RC0402-10K" ) );
    BOOST_CHECK( issue.issue_type == wxS( "missing" ) );
    BOOST_CHECK( issue.message == wxS( "Part not in database" ) );
    BOOST_CHECK( issue.suggested_part.IsEmpty() );
}


/**
 * Test PLM_BOM_ISSUE::Missing factory.
 */
BOOST_AUTO_TEST_CASE( BomIssueMissing )
{
    auto issue = PLM_BOM_ISSUE::Missing( wxS( "U1" ), wxS( "IC-12345" ) );

    BOOST_CHECK( issue.reference == wxS( "U1" ) );
    BOOST_CHECK( issue.part_number == wxS( "IC-12345" ) );
    BOOST_CHECK( issue.issue_type == wxS( "missing" ) );
    BOOST_CHECK( !issue.message.IsEmpty() );
}


/**
 * Test PLM_BOM_ISSUE::Obsolete factory.
 */
BOOST_AUTO_TEST_CASE( BomIssueObsolete )
{
    auto issue = PLM_BOM_ISSUE::Obsolete( wxS( "C1" ), wxS( "CAP-OLD" ), wxS( "CAP-NEW" ) );

    BOOST_CHECK( issue.reference == wxS( "C1" ) );
    BOOST_CHECK( issue.part_number == wxS( "CAP-OLD" ) );
    BOOST_CHECK( issue.issue_type == wxS( "obsolete" ) );
    BOOST_CHECK( issue.suggested_part == wxS( "CAP-NEW" ) );
    BOOST_CHECK( issue.part_state == PLM_PART_STATE::OBSOLETE );
}


/**
 * Test PLM_BOM_ISSUE::NotRecommended factory.
 */
BOOST_AUTO_TEST_CASE( BomIssueNotRecommended )
{
    auto issue = PLM_BOM_ISSUE::NotRecommended( wxS( "R5" ), wxS( "RES-OLD" ), wxS( "RES-NEW" ) );

    BOOST_CHECK( issue.reference == wxS( "R5" ) );
    BOOST_CHECK( issue.issue_type == wxS( "not_recommended" ) );
    BOOST_CHECK( issue.part_state == PLM_PART_STATE::NOT_RECOMMENDED );
}


/**
 * Test PLM_BOM_VALIDATION_RESULT default construction.
 */
BOOST_AUTO_TEST_CASE( ValidationResultDefaults )
{
    PLM_BOM_VALIDATION_RESULT result;

    BOOST_CHECK( result.all_valid );
    BOOST_CHECK( result.issues.empty() );
    BOOST_CHECK_EQUAL( result.total_parts, 0 );
    BOOST_CHECK_EQUAL( result.valid_parts, 0 );
    BOOST_CHECK_EQUAL( result.missing_count, 0 );
    BOOST_CHECK_EQUAL( result.obsolete_count, 0 );
    BOOST_CHECK_EQUAL( result.not_recommended_count, 0 );
}


/**
 * Test PLM_BOM_VALIDATION_RESULT::Success factory.
 */
BOOST_AUTO_TEST_CASE( ValidationResultSuccess )
{
    auto result = PLM_BOM_VALIDATION_RESULT::Success( 50 );

    BOOST_CHECK( result.all_valid );
    BOOST_CHECK_EQUAL( result.total_parts, 50 );
    BOOST_CHECK_EQUAL( result.valid_parts, 50 );
    BOOST_CHECK_EQUAL( result.GetPassRate(), 100.0 );
    BOOST_CHECK( !result.HasCriticalIssues() );
}


/**
 * Test adding issues to validation result.
 */
BOOST_AUTO_TEST_CASE( ValidationResultAddIssues )
{
    PLM_BOM_VALIDATION_RESULT result;
    result.total_parts = 10;
    result.valid_parts = 7;

    result.AddIssue( PLM_BOM_ISSUE::Missing( wxS( "R1" ), wxS( "PART1" ) ) );
    result.AddIssue( PLM_BOM_ISSUE::Obsolete( wxS( "C1" ), wxS( "PART2" ) ) );
    result.AddIssue( PLM_BOM_ISSUE::NotRecommended( wxS( "U1" ), wxS( "PART3" ) ) );

    BOOST_CHECK( !result.all_valid );
    BOOST_CHECK_EQUAL( result.issues.size(), 3 );
    BOOST_CHECK_EQUAL( result.missing_count, 1 );
    BOOST_CHECK_EQUAL( result.obsolete_count, 1 );
    BOOST_CHECK_EQUAL( result.not_recommended_count, 1 );
    BOOST_CHECK( result.HasCriticalIssues() );
}


/**
 * Test filtering issues by type.
 */
BOOST_AUTO_TEST_CASE( ValidationResultFilterIssues )
{
    PLM_BOM_VALIDATION_RESULT result;

    result.AddIssue( PLM_BOM_ISSUE::Missing( wxS( "R1" ), wxS( "PART1" ) ) );
    result.AddIssue( PLM_BOM_ISSUE::Missing( wxS( "R2" ), wxS( "PART2" ) ) );
    result.AddIssue( PLM_BOM_ISSUE::Obsolete( wxS( "C1" ), wxS( "PART3" ) ) );

    auto missingIssues = result.GetIssuesByType( wxS( "missing" ) );
    BOOST_CHECK_EQUAL( missingIssues.size(), 2 );

    auto obsoleteIssues = result.GetIssuesByType( wxS( "obsolete" ) );
    BOOST_CHECK_EQUAL( obsoleteIssues.size(), 1 );

    auto notRecIssues = result.GetIssuesByType( wxS( "not_recommended" ) );
    BOOST_CHECK( notRecIssues.empty() );
}


/**
 * Test validation result pass rate calculation.
 */
BOOST_AUTO_TEST_CASE( ValidationResultPassRate )
{
    PLM_BOM_VALIDATION_RESULT result;
    result.total_parts = 100;
    result.valid_parts = 75;

    BOOST_CHECK_EQUAL( result.GetPassRate(), 75.0 );

    result.valid_parts = 100;
    BOOST_CHECK_EQUAL( result.GetPassRate(), 100.0 );

    result.valid_parts = 0;
    BOOST_CHECK_EQUAL( result.GetPassRate(), 0.0 );

    result.total_parts = 0;
    BOOST_CHECK_EQUAL( result.GetPassRate(), 100.0 );
}


/**
 * Test critical issues detection.
 */
BOOST_AUTO_TEST_CASE( ValidationResultCriticalIssues )
{
    PLM_BOM_VALIDATION_RESULT result;

    BOOST_CHECK( !result.HasCriticalIssues() );

    result.AddIssue( PLM_BOM_ISSUE::NotRecommended( wxS( "R1" ), wxS( "PART1" ) ) );
    BOOST_CHECK( !result.HasCriticalIssues() );

    result.AddIssue( PLM_BOM_ISSUE::Missing( wxS( "R2" ), wxS( "PART2" ) ) );
    BOOST_CHECK( result.HasCriticalIssues() );
}


/**
 * Test PLM_PART_REPLACEMENT construction.
 */
BOOST_AUTO_TEST_CASE( PartReplacementConstruction )
{
    PLM_PART_REPLACEMENT repl;

    BOOST_CHECK( repl.original_part_id.IsEmpty() );
    BOOST_CHECK( repl.replacement_part_id.IsEmpty() );
    BOOST_CHECK( !repl.form_fit_function );
    BOOST_CHECK( !repl.verified );
}


/**
 * Test PLM_PART_REPLACEMENT with parameters.
 */
BOOST_AUTO_TEST_CASE( PartReplacementWithParams )
{
    PLM_PART_REPLACEMENT repl( wxS( "OLD-001" ), wxS( "NEW-001" ), wxS( "Updated package" ) );

    BOOST_CHECK( repl.original_part_id == wxS( "OLD-001" ) );
    BOOST_CHECK( repl.replacement_part_id == wxS( "NEW-001" ) );
    BOOST_CHECK( repl.reason == wxS( "Updated package" ) );
}


/**
 * Test part with all attributes set.
 */
BOOST_AUTO_TEST_CASE( PartFullAttributes )
{
    PLM_PART part;
    part.part_id = wxS( "part-full" );
    part.part_number = wxS( "RESISTOR-10K-0402" );
    part.manufacturer = wxS( "Yageo" );
    part.manufacturer_pn = wxS( "RC0402FR-0710KL" );
    part.description = wxS( "Resistor 10K Ohm 1% 0402" );
    part.category = wxS( "Passives" );
    part.subcategory = wxS( "Resistors" );
    part.state = PLM_PART_STATE::PREFERRED;
    part.datasheet_url = wxS( "https://example.com/datasheet.pdf" );
    part.symbol_ref = wxS( "Device:R" );
    part.footprint_ref = wxS( "Resistor_SMD:R_0402_1005Metric" );
    part.created_at = 1704067200;
    part.modified_at = 1704153600;

    BOOST_CHECK( part.IsAvailable() );
    BOOST_CHECK( !part.NeedsReplacement() );
    BOOST_CHECK( part.GetStateName() == wxS( "Preferred" ) );
    BOOST_CHECK( !part.symbol_ref.IsEmpty() );
    BOOST_CHECK( !part.footprint_ref.IsEmpty() );
}


BOOST_AUTO_TEST_SUITE_END()
