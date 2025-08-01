#include "construction.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <numeric>
#include <utility>

#include "action.h"
#include "avatar.h"
#include "build_reqs.h"
#include "calendar.h"
#include "cata_scope_helpers.h"
#include "cata_utility.h"
#include "character.h"
#include "color.h"
#include "construction_category.h"
#include "construction_group.h"
#include "coordinates.h"
#include "crafting.h"
#include "creature.h"
#include "cursesdef.h"
#include "debug.h"
#include "enums.h"
#include "event.h"
#include "event_bus.h"
#include "flexbuffer_json.h"
#include "game.h"
#include "game_constants.h"
#include "input.h"
#include "input_context.h"
#include "inventory.h"
#include "item.h"
#include "item_group.h"
#include "iteminfo_query.h"
#include "iuse.h"
#include "map.h"
#include "map_iterator.h"
#include "map_scale_constants.h"
#include "mapdata.h"
#include "memory_fast.h"
#include "messages.h"
#include "mtype.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "panels.h"
#include "player_activity.h"
#include "point.h"
#include "requirements.h"
#include "rng.h"
#include "skill.h"
#include "sounds.h"
#include "string_formatter.h"
#include "string_input_popup.h"
#include "translation_cache.h"
#include "translations.h"
#include "trap.h"
#include "ui_manager.h"
#include "uilist.h"
#include "uistate.h"
#include "units.h"
#include "veh_appliance.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vpart_position.h"

#if defined(TILES)
#include "cached_options.h"    // for use_tiles
#include "cata_tiles.h"        // for cata_tiles
#include "cuboid_rectangle.h"  // for half_open_rectangle
#include "cursesport.h"        // for get_scaling_factor (??)
#include "sdltiles.h"          // for tilecontext
#endif

class read_only_visitable;

static const activity_id ACT_BUILD( "ACT_BUILD" );
static const activity_id ACT_MULTIPLE_CONSTRUCTION( "ACT_MULTIPLE_CONSTRUCTION" );

static const construction_category_id construction_category_ALL( "ALL" );
static const construction_category_id construction_category_APPLIANCE( "APPLIANCE" );
static const construction_category_id construction_category_DECONSTRUCT( "DECONSTRUCT" );
static const construction_category_id construction_category_DECORATE( "DECORATE" );
static const construction_category_id construction_category_FILTER( "FILTER" );
static const construction_category_id construction_category_REPAIR( "REPAIR" );

static const construction_group_str_id
construction_group_deconstruct_simple_furniture( "deconstruct_simple_furniture" );

static const construction_str_id construction_constr_veh( "constr_veh" );


static const flag_id json_flag_FILTHY( "FILTHY" );

static const furn_str_id furn_f_coffin_c( "f_coffin_c" );
static const furn_str_id furn_f_coffin_o( "f_coffin_o" );

static const item_group_id Item_spawn_data_allclothes( "allclothes" );
static const item_group_id Item_spawn_data_grave( "grave" );
static const item_group_id Item_spawn_data_jewelry_front( "jewelry_front" );

static const itype_id itype_2x4( "2x4" );
static const itype_id itype_bone_human( "bone_human" );
static const itype_id itype_nail( "nail" );
static const itype_id itype_rope_30( "rope_30" );
static const itype_id itype_sheet( "sheet" );
static const itype_id itype_stick( "stick" );
static const itype_id itype_string_36( "string_36" );
static const itype_id itype_wall_wiring( "wall_wiring" );

static const morale_type morale_funeral( "morale_funeral" );
static const morale_type morale_gravedigger( "morale_gravedigger" );

static const mtype_id mon_skeleton( "mon_skeleton" );
static const mtype_id mon_zombie( "mon_zombie" );
static const mtype_id mon_zombie_crawler( "mon_zombie_crawler" );
static const mtype_id mon_zombie_fat( "mon_zombie_fat" );
static const mtype_id mon_zombie_rot( "mon_zombie_rot" );

static const quality_id qual_CUT( "CUT" );

static const skill_id skill_fabrication( "fabrication" );

static const ter_str_id ter_t_clay( "t_clay" );
static const ter_str_id ter_t_dirt( "t_dirt" );
static const ter_str_id ter_t_ladder_up( "t_ladder_up" );
static const ter_str_id ter_t_lava( "t_lava" );
static const ter_str_id ter_t_open_air( "t_open_air" );
static const ter_str_id ter_t_pit( "t_pit" );
static const ter_str_id ter_t_ramp_down_high( "t_ramp_down_high" );
static const ter_str_id ter_t_ramp_down_low( "t_ramp_down_low" );
static const ter_str_id ter_t_rock_floor( "t_rock_floor" );
static const ter_str_id ter_t_sand( "t_sand" );
static const ter_str_id ter_t_stairs_down( "t_stairs_down" );
static const ter_str_id ter_t_stairs_up( "t_stairs_up" );
static const ter_str_id ter_t_wood_stairs_down( "t_wood_stairs_down" );

static const trait_id trait_DEBUG_HS( "DEBUG_HS" );
static const trait_id trait_EATDEAD( "EATDEAD" );
static const trait_id trait_NUMB( "NUMB" );
static const trait_id trait_PAINRESIST_TROGLO( "PAINRESIST_TROGLO" );
static const trait_id trait_PSYCHOPATH( "PSYCHOPATH" );
static const trait_id trait_SAPROVORE( "SAPROVORE" );
static const trait_id trait_SPIRITUAL( "SPIRITUAL" );
static const trait_id trait_STOCKY_TROGLO( "STOCKY_TROGLO" );

static const trap_str_id tr_firewood_source( "tr_firewood_source" );
static const trap_str_id tr_practice_target( "tr_practice_target" );

static const vpart_id vpart_frame( "frame" );

static const vproto_id vehicle_prototype_none( "none" );

static const std::string flag_INITIAL_PART( "INITIAL_PART" );
static const std::string flag_WIRING( "WIRING" );

static bool finalized = false;

// Construction functions.
namespace construct
{
// Checks for whether terrain mod can proceed
static bool check_nothing( const tripoint_bub_ms & )
{
    return true;
}
static bool check_channel( const tripoint_bub_ms & ); // tile has adjacent flowing water
static bool check_empty_lite( const tripoint_bub_ms & );
static bool check_empty( const tripoint_bub_ms & ); // tile is empty
static bool check_unblocked( const tripoint_bub_ms & ); // tile is empty or empty space
static bool check_support( const tripoint_bub_ms
                           & ); // at least two orthogonal supports or from below
static bool check_support_below( const tripoint_bub_ms
                                 & ); // at least two orthogonal supports at the level below or from below
static bool check_single_support( const tripoint_bub_ms
                                  &p ); // Only support from directly below matters
static bool check_stable( const tripoint_bub_ms & ); // tile below has a flag SUPPORTS_ROOF
static bool check_nofloor_above( const tripoint_bub_ms & ); // tile above has a flag NO_FLOOR
static bool check_deconstruct( const tripoint_bub_ms
                               & ); // either terrain or furniture must be deconstructible
static bool check_up_OK( const tripoint_bub_ms & ); // tile is below OVERMAP_HEIGHT
static bool check_down_OK( const tripoint_bub_ms & ); // tile is above OVERMAP_DEPTH
static bool check_no_trap( const tripoint_bub_ms & ); // tile doesn't contain any trap
static bool check_ramp_high( const tripoint_bub_ms
                             & ); // one of the adjacent tiles on the z-level above has a completed down ramp
static bool check_no_wiring( const tripoint_bub_ms
                             & ); // tile doesn't contain appliances/vehicle parts with WIRING flag like ap_wall_wiring
static bool check_matching_down_above( const tripoint_bub_ms
                                       &p ); // tile above has the same base name but with the "down suffix

// Special actions to be run post-terrain-mod
static void done_nothing( const tripoint_bub_ms &, Character & ) {}
static void done_trunk_plank( const tripoint_bub_ms &, Character & );
static void done_grave( const tripoint_bub_ms &, Character & );
static void done_vehicle( const tripoint_bub_ms &, Character & );
static void done_appliance( const tripoint_bub_ms &, Character & );
static void done_wiring( const tripoint_bub_ms &, Character & );
static void done_deconstruct( const tripoint_bub_ms &, Character & );
static void done_digormine_stair( const tripoint_bub_ms &, bool, Character & );
static void done_dig_grave( const tripoint_bub_ms &p, Character & );
static void done_dig_grave_nospawn( const tripoint_bub_ms &p, Character & );
static void done_dig_stair( const tripoint_bub_ms &, Character & );
static void done_mine_downstair( const tripoint_bub_ms &, Character & );
static void done_mine_upstair( const tripoint_bub_ms &, Character & );
static void done_wood_stairs( const tripoint_bub_ms &, Character & );
static void done_window_curtains( const tripoint_bub_ms &, Character & );
static void done_extract_maybe_revert_to_dirt( const tripoint_bub_ms &, Character & );
static void done_mark_firewood( const tripoint_bub_ms &, Character & );
static void done_mark_practice_target( const tripoint_bub_ms &, Character & );
static void done_ramp_low( const tripoint_bub_ms &, Character & );
static void done_ramp_high( const tripoint_bub_ms &, Character & );
static void add_matching_down_above( const tripoint_bub_ms &p, Character & );
static void remove_above( const tripoint_bub_ms &p, Character & );
static void add_roof( const tripoint_bub_ms &p, Character & );

static void do_turn_deconstruct( const tripoint_bub_ms &, Character & );
static void do_turn_shovel( const tripoint_bub_ms &, Character & );
static void do_turn_exhume( const tripoint_bub_ms &, Character & );

static void failure_standard( const tripoint_bub_ms & );
static void failure_deconstruct( const tripoint_bub_ms & );
} // namespace construct

static std::vector<construction> constructions;
static std::map<construction_str_id, construction_id> construction_id_map;

// Helper functions, nobody but us needs to call these.
static bool can_construct( const construction &con );
static std::vector<construction *> player_can_build_valid_constructions( Character &you,
        const read_only_visitable &inv, const construction_group_str_id &group );
static bool player_can_build( Character &you, const read_only_visitable &inv,
                              const construction_group_str_id &group );
static bool player_can_see_to_build( Character &you, const construction_group_str_id &group );

// Color standardization for string streams
static const deferred_color color_title = def_c_light_red; //color for titles
static const deferred_color color_data = def_c_cyan; //color for data parts

static bool has_pre_terrain( const construction &con, const tripoint_bub_ms &p )
{
    if( con.pre_terrain.empty() ) {
        return true;
    }

    map &here = get_map();
    if( con.pre_is_furniture ) {
        furn_id f = here.furn( p );
        for( const auto &pre_terrain : con.pre_terrain ) {
            if( f == furn_id( pre_terrain ) ) {
                return true;
            }
        }
    } else {
        ter_id t = here.ter( p );
        for( const auto &pre_terrain : con.pre_terrain ) {
            if( t == ter_id( pre_terrain ) ) {
                return true;
            }
        }
    }
    return false;
}

static bool has_pre_terrain( const construction &con )
{
    if( con.pre_terrain.empty() ) {
        return true;
    }

    tripoint_bub_ms avatar_pos = get_player_character().pos_bub();
    for( const tripoint_bub_ms &p : get_map().points_in_radius( avatar_pos, 1 ) ) {
        if( p != avatar_pos && has_pre_terrain( con, p ) ) {
            return true;
        }
    }
    return false;
}

void standardize_construction_times( const int time )
{
    if( !finalized ) {
        debugmsg( "standardize_construction_times called before finalization" );
        return;
    }
    for( construction &c : constructions ) {
        c.time = time;
    }
}

std::vector<construction *> constructions_by_group( const construction_group_str_id &group )
{
    return constructions_by_filter(
    [&group]( construction const & it ) {
        return it.group == group;
    } );
}

std::vector<construction *>
constructions_by_filter( std::function<bool( construction const & )> const &filter )
{
    if( !finalized ) {
        debugmsg( "constructions_by_filter called before finalization" );
        return {};
    }
    std::vector<construction *> result;
    for( construction &constructions_a : constructions ) {
        if( filter( constructions_a ) ) {
            result.push_back( &constructions_a );
        }
    }
    return result;
}

static void load_available_constructions( std::vector<construction_group_str_id> &available,
        std::map<construction_category_id, std::vector<construction_group_str_id>> &cat_available,
        bool hide_unconstructable )
{
    cat_available.clear();
    available.clear();
    if( !finalized ) {
        debugmsg( "load_available_constructions called before finalization" );
        return;
    }
    avatar &player_character = get_avatar();
    for( construction &it : constructions ) {
        if( it.on_display && ( !hide_unconstructable ||
                               player_can_build( player_character, player_character.crafting_inventory(), it ) ) ) {
            bool already_have_it = false;
            for( auto &avail_it : available ) {
                if( avail_it == it.group ) {
                    already_have_it = true;
                    break;
                }
            }
            if( !already_have_it ) {
                available.push_back( it.group );
                cat_available[it.category].push_back( it.group );
            }
        }
    }
}

static void draw_grid( const catacurses::window &w, const int list_width )
{
    draw_border( w );
    mvwprintz( w, point( 2, 0 ), c_light_red, _( " Construction " ) );

    wattron( w, c_light_gray );

    // draw internal lines
    mvwvline( w, point( list_width, 1 ), LINE_XOXO, getmaxy( w ) - 2 );
    mvwhline( w, point( 1, 2 ), LINE_OXOX, list_width );
    // draw intersections
    mvwaddch( w, point( list_width, 0 ), LINE_OXXX );
    mvwaddch( w, point( list_width, getmaxy( w ) - 1 ), LINE_XXOX );
    mvwaddch( w, point( 0, 2 ), LINE_XXXO );
    mvwaddch( w, point( list_width, 2 ), LINE_XOXX );

    wattroff( w, c_light_gray );

    wnoutrefresh( w );
}

static nc_color construction_color( const construction_group_str_id &group, bool highlight )
{
    nc_color col = c_dark_gray;
    Character &player_character = get_player_character();
    if( player_character.has_trait( trait_DEBUG_HS ) ) {
        col = c_white;
    } else {
        std::vector<construction *> cons = player_can_build_valid_constructions( player_character,
                                           player_character.crafting_inventory(), group );
        if( !cons.empty() ) {
            col = c_white;
            for( const auto &pr : cons.front()->required_skills ) {
                int s_lvl = player_character.get_skill_level( pr.first );
                if( s_lvl < pr.second ) {
                    col = c_red;
                } else if( s_lvl < pr.second * 1.25 ) {
                    col = c_light_blue;
                }
            }
        }
    }
    return highlight ? hilite( col ) : col;
}

const std::vector<construction> &get_constructions()
{
    if( !finalized ) {
        debugmsg( "get_constructions called before finalization" );
        static std::vector<construction> fake_constructions;
        return fake_constructions;
    }
    return constructions;
}

static std::string furniture_qualities_string( const furn_id &fid )
{
    std::string ret = "\n";
    // Make a pseudo item instance so we can use qualities_info later
    const item pseudo( fid->crafting_pseudo_item );
    // Set up iteminfo query to show qualities
    std::vector<iteminfo_parts> quality_part = { iteminfo_parts::QUALITIES };
    const iteminfo_query quality_query( quality_part );
    // Render info into info_vec
    std::vector<iteminfo> info_vec;
    pseudo.qualities_info( info_vec, &quality_query, 1, false );
    // Get a newline-separated string of quality info, then parse and print each line
    ret += format_item_info( info_vec, {} );

    return ret;
}

static std::pair<std::map<tripoint_bub_ms, const construction *>, std::vector<construction *>>
        valid_constructions_near_player( const std::vector<construction_group_str_id> &groups,
                const inventory &total_inv, avatar &player_character );

static shared_ptr_fast<game::draw_callback_t> construction_preview_callback(
    const std::map<tripoint_bub_ms, const construction *> &valid,
    const std::optional<tripoint_bub_ms> &mouse_pos, const bool &blink )
{
    return make_shared_fast<game::draw_callback_t>( [&]() {
        map &here = get_map();
        // Draw construction result preview on valid squares
        for( const auto &elem : valid ) {
            const tripoint_bub_ms &loc = elem.first;
            const construction &con = *elem.second;
            const std::string &post_id = con.post_terrain;
            const bool preview = !mouse_pos.has_value() || mouse_pos.value() == loc;
            if( !post_id.empty() ) {
                if( con.post_is_furniture ) {
                    if( is_draw_tiles_mode() ) {
                        if( blink && preview ) {
                            g->draw_furniture_override( loc, furn_str_id( post_id ) );
                        }
                        g->draw_highlight( loc );
                    } else {
                        here.drawsq( g->w_terrain, loc,
                                     drawsq_params().highlight( true )
                                     .show_items( true )
                                     .furniture_override( blink && preview
                                                          ? furn_str_id( post_id )
                                                          : furn_str_id::NULL_ID() ) );
                    }
                } else {
                    if( is_draw_tiles_mode() ) {
                        if( blink && preview ) {
                            g->draw_terrain_override( loc, ter_str_id( post_id ) );
                        }
                        g->draw_highlight( loc );
                    } else {
                        here.drawsq( g->w_terrain, loc,
                                     drawsq_params().highlight( true )
                                     .show_items( true )
                                     .terrain_override( blink && preview
                                                        ? ter_str_id( post_id )
                                                        : ter_str_id::NULL_ID() ) );
                    }
                }
            } else {
                if( is_draw_tiles_mode() ) {
                    g->draw_highlight( loc );
                } else {
                    here.drawsq( g->w_terrain, loc,
                                 drawsq_params().highlight( true )
                                 .show_items( true ) );
                }
            }
        }
    } );
}

construction_id construction_menu( const bool blueprint )
{
    if( !finalized ) {
        debugmsg( "construction_menu called before finalization" );
        return construction_id( -1 );
    }
    static bool hide_unconstructable = false;
    // only display constructions the player can theoretically perform
    std::vector<construction_group_str_id> available;
    std::map<construction_category_id, std::vector<construction_group_str_id>> cat_available;
    load_available_constructions( available, cat_available, hide_unconstructable );

    if( available.empty() ) {
        popup( _( "You can not construct anything here." ) );
        return construction_id( -1 );
    }

    int w_height = 0;
    int w_width = 0;
    catacurses::window w_con;

    int w_list_width = 0;
    int w_list_height = 0;
    const int w_list_x0 = 1;
    catacurses::window w_list;

    std::vector<std::string> notes;
    int pos_x = 0;
    int available_window_width = 0;
    int available_buffer_height = 0;

    construction_id ret( -1 );

    bool update_info = true;
    bool update_cat = true;
    bool isnew = true;
    int tabindex = 0;
    int select = 0;
    int offset = 0;
    bool exit = false;
    construction_category_id category_id;
    std::vector<construction_group_str_id> constructs;
    //storage for the color text so it can be scrolled
    std::vector< std::vector < std::string > > construct_buffers;
    std::vector<std::string> full_construct_buffer;
    std::vector<int> construct_buffer_breakpoints;
    int total_project_breakpoints = 0;
    int current_construct_breakpoint = 0;
    avatar &player_character = get_avatar();
    const inventory &total_inv = player_character.crafting_inventory();

    input_context ctxt( "CONSTRUCTION" );
    ctxt.register_navigate_ui_list();
    ctxt.register_leftright();
    ctxt.register_action( "NEXT_TAB" );
    ctxt.register_action( "PREV_TAB" );
    ctxt.register_action( "SCROLL_STAGE_UP" );
    ctxt.register_action( "SCROLL_STAGE_DOWN" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "TOGGLE_UNAVAILABLE_CONSTRUCTIONS" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "FILTER" );
    ctxt.register_action( "RESET_FILTER" );
    ctxt.set_timeout( get_option<int>( "BLINK_SPEED" ) );

    const std::vector<construction_category> &construct_cat = construction_categories::get_all();
    const int tabcount = static_cast<int>( construction_category::count() );

    std::string filter;

    const nc_color color_stage = c_white;
    ui_adaptor ui;

    std::unique_ptr<on_out_of_scope> restore_ui = std::make_unique<on_out_of_scope>( []() {
#if defined( TILES )
        tilecontext->set_disable_occlusion( false );
#endif
        // always needs to restore view
        g->invalidate_main_ui_adaptor();
    } );
#if defined( TILES )
    tilecontext->set_disable_occlusion( true );
    g->invalidate_main_ui_adaptor();
#endif
    std::unique_ptr<restore_on_out_of_scope<tripoint_rel_ms>> restore_view =
                std::make_unique<restore_on_out_of_scope<tripoint_rel_ms>>( player_character.view_offset );

    const auto recalc_buffer = [&]() {
        //leave room for top and bottom UI text
        available_buffer_height = w_height - 3 - 3 - static_cast<int>( notes.size() );

        if( !constructs.empty() ) {
            if( select >= static_cast<int>( constructs.size() ) ) {
                select = 0;
            }
            const construction_group_str_id &current_group = constructs[select];

            //construct the project list buffer

            // Print stages and their requirement.
            std::vector<construction *> options = constructions_by_group( current_group );

            construct_buffers.clear();
            current_construct_breakpoint = 0;
            construct_buffer_breakpoints.clear();
            full_construct_buffer.clear();
            int stage_counter = 0;
            for( std::vector<construction *>::iterator it = options.begin();
                 it != options.end(); ++it ) {
                stage_counter++;
                construction *current_con = *it;
                if( hide_unconstructable && !can_construct( *current_con ) ) {
                    continue;
                }
                // Update the cached availability of components and tools in the requirement object
                current_con->requirements->can_make_with_inventory( total_inv, is_crafting_component, 1,
                        craft_flags::none, false );

                std::vector<std::string> current_buffer;

                const auto add_folded = [&]( const std::vector<std::string> &folded ) {
                    current_buffer.insert( current_buffer.end(), folded.begin(), folded.end() );
                };
                const auto add_line = [&]( const std::string & line ) {
                    add_folded( foldstring( line, available_window_width ) );
                };

                // display final product name only if more than one step.
                // Assume single stage constructions should be clear
                // in their title what their result is.
                if( !current_con->post_terrain.empty() && options.size() > 1 ) {
                    //also print out stage number when multiple stages are available
                    std::string current_line = string_format( _( "Stage/Variant #%d: " ), stage_counter );

                    // print name of the result of each stage
                    std::string result_string;
                    if( current_con->post_is_furniture ) {
                        result_string = furn_str_id( current_con->post_terrain ).obj().name();
                    } else {
                        result_string = ter_str_id( current_con->post_terrain ).obj().name();
                    }
                    current_line += colorize( result_string, color_title );
                    add_line( current_line );

                    // display description of the result for multi-stages
                    current_line = _( "Result: " );
                    if( current_con->post_is_furniture ) {
                        current_line += colorize(
                                            furn_str_id( current_con->post_terrain ).obj().description,
                                            color_data
                                        );
                        furn_id fid( current_con->post_terrain );
                        if( !fid->crafting_pseudo_item.is_empty() ) {
                            current_line += furniture_qualities_string( fid );
                        }
                    } else {
                        current_line += colorize(
                                            ter_str_id( current_con->post_terrain ).obj().description,
                                            color_data
                                        );
                    }
                    add_line( current_line );

                    // display description of the result for single stages
                } else if( !current_con->post_terrain.empty() ) {
                    std::string current_line = _( "Result: " );
                    if( current_con->post_is_furniture ) {
                        current_line += colorize(
                                            furn_str_id( current_con->post_terrain ).obj().description,
                                            color_data
                                        );
                        furn_id fid( current_con->post_terrain );
                        if( !fid->crafting_pseudo_item.is_empty() ) {
                            current_line += furniture_qualities_string( fid );
                        }
                    } else {
                        current_line += colorize(
                                            ter_str_id( current_con->post_terrain ).obj().description,
                                            color_data
                                        );
                    }
                    add_line( current_line );
                }

                // display required skill and difficulty
                if( current_con->required_skills.empty() ) {
                    add_line( _( "N/A" ) );
                } else {
                    std::string current_line = _( "Required skills: " ) + enumerate_as_string(
                                                   current_con->required_skills.begin(), current_con->required_skills.end(),
                    [&player_character]( const std::pair<skill_id, int> &skill ) {
                        nc_color col;
                        int s_lvl = player_character.get_skill_level( skill.first );
                        if( s_lvl < skill.second ) {
                            col = c_red;
                        } else if( s_lvl < skill.second * 1.25 ) {
                            col = c_light_blue;
                        } else {
                            col = c_green;
                        }

                        return colorize( string_format( "%s (%d)", skill.first.obj().name(), skill.second ), col );
                    }, enumeration_conjunction::none );
                    add_line( current_line );
                }

                // TODO: Textify pre_flags to provide a bit more information.
                // Example: First step of dig pit could say something about
                // requiring diggable ground.
                if( !current_con->pre_terrain.empty() ) {
                    // C++20 will allow this to be generated on demand with a std::ranges::views::transform
                    std::vector<std::string> require_names( current_con->pre_terrain.size() );
                    if( current_con->pre_is_furniture ) {
                        std::transform( current_con->pre_terrain.cbegin(), current_con->pre_terrain.cend(),
                        require_names.begin(), []( std::string pre_terrain ) {
                            return furn_str_id( pre_terrain )->name();
                        } );
                    } else {
                        std::transform( current_con->pre_terrain.cbegin(), current_con->pre_terrain.cend(),
                        require_names.begin(), []( std::string pre_terrain ) {
                            return ter_str_id( pre_terrain )->name();
                        } );
                    }
                    nc_color pre_color = has_pre_terrain( *current_con ) ? c_green : c_red;
                    add_line( _( "Requires: " ) + colorize( enumerate_as_string( require_names,
                                                            enumeration_conjunction::or_ ), pre_color ) );
                }
                if( !current_con->pre_note.empty() ) {
                    add_line( _( "Annotation: " ) + colorize( current_con->pre_note, color_data ) );
                }
                // get pre-folded versions of the rest of the construction project to be displayed later

                // get time needed
                add_folded( current_con->get_folded_time_string( available_window_width ) );

                add_folded( current_con->requirements->get_folded_tools_list( available_window_width, color_stage,
                            total_inv ) );

                add_folded( current_con->requirements->get_folded_components_list( available_window_width,
                            color_stage, total_inv, is_crafting_component ) );

                construct_buffers.push_back( current_buffer );
            }

            //determine where the printing starts for each project, so it can be scrolled to those points
            size_t current_buffer_location = 0;
            for( size_t i = 0; i < construct_buffers.size(); i++ ) {
                construct_buffer_breakpoints.push_back( static_cast<int>( current_buffer_location ) );
                full_construct_buffer.insert( full_construct_buffer.end(), construct_buffers[i].begin(),
                                              construct_buffers[i].end() );

                //handle text too large for one screen
                if( construct_buffers[i].size() > static_cast<size_t>( available_buffer_height ) ) {
                    construct_buffer_breakpoints.push_back( static_cast<int>( current_buffer_location +
                                                            static_cast<size_t>( available_buffer_height ) ) );
                }
                current_buffer_location += construct_buffers[i].size();
                if( i < construct_buffers.size() - 1 ) {
                    full_construct_buffer.emplace_back( );
                    current_buffer_location++;
                }
            }
            total_project_breakpoints = static_cast<int>( construct_buffer_breakpoints.size() );
        }
    };

    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        const int left_panel_width = panel_manager::get_manager().get_width_left();
        const int right_panel_width = panel_manager::get_manager().get_width_right();

        // 3 tiles of valid construction locations around the player character,
        // plus 2 tiles of padding.
        const int reserve_tile_height = 5;
#if defined( TILES )
        const window_dimensions ter_dims = get_window_dimensions( g->w_terrain );
        const window_dimensions norm_dims = get_window_dimensions( catacurses::stdscr );
        const int tile_height = g->is_tileset_isometric()
                                ? ter_dims.scaled_font_size.x / 2 : ter_dims.scaled_font_size.y;
        const int tile_extra_height = use_tiles
                                      ? tilecontext->get_max_tile_extent().p_max.y * get_scaling_factor()
                                      - tile_height
                                      : 0;
        // desired number of reserved lines from the construction menu.
        const int reserve_height = ( ( reserve_tile_height
                                       // reserve the minimum number of full tiles that makes the
                                       // extra tile height visible.
                                       + ( tile_extra_height + tile_height - 1 ) / tile_height )
                                     // reserve the minimum number of construction menu lines that
                                     // makes the reserved tiles visible.
                                     * tile_height + norm_dims.scaled_font_size.y - 1 )
                                   / norm_dims.scaled_font_size.y;
#else
        // desired number of reserved lines from the construction menu.
        const int reserve_height = reserve_tile_height;
#endif

        w_height = TERMY - reserve_height;
        // reduce height to match content
        if( static_cast<int>( available.size() ) + 2 < w_height ) {
            w_height = available.size() + 2;
        }
        // but no lower than FULL_SCREEN_HEIGHT
        if( w_height < FULL_SCREEN_HEIGHT ) {
            w_height = FULL_SCREEN_HEIGHT;
        }
        // if there is no enough space for main UI, hide it
        if( w_height + reserve_height > TERMY ) {
            w_height = std::max( TERMY, FULL_SCREEN_HEIGHT );
        }
        // number of normal lines for main UI (may be larger than reserve_height)
        const int w_y0 = TERMY - w_height;

        // keep side panel visible
        w_width = TERMX - left_panel_width - right_panel_width;
        int w_x0 = left_panel_width;
        // unless width is too small
        if( w_width < FULL_SCREEN_WIDTH ) {
            w_width = std::max( TERMX, FULL_SCREEN_WIDTH );
            w_x0 = 0;
        }

        w_con = catacurses::newwin( w_height, w_width, point( w_x0, w_y0 ) );

        // center player tile in the visible part of main UI
#if defined( TILES )
        const point visible_center(
            ter_dims.window_size_pixel.x / 2,
            ( norm_dims.scaled_font_size.y * w_y0
              // shift up to reveal the full height of the topmost tile
              + ( tile_extra_height + tile_height - 1 ) / tile_height * tile_height ) / 2 );
        const point_bub_ms target = cata_tiles::screen_to_player(
                                        visible_center,
                                        ter_dims.scaled_font_size, ter_dims.window_size_pixel,
                                        player_character.pos_bub().xy(), g->is_tileset_isometric() );
        player_character.view_offset = tripoint_rel_ms( player_character.pos_bub().xy() - target, 0 );
#else
        player_character.view_offset = tripoint_rel_ms( 0, ( w_height + 1 ) / 2, 0 );
#endif
        g->invalidate_main_ui_adaptor();

        w_list_width = static_cast<int>( .375 * w_width );
        w_list_height = w_height - 4;
        w_list = catacurses::newwin( w_list_height, w_list_width,
                                     point( w_x0 + w_list_x0, w_y0 + 3 ) );

        pos_x = w_list_width + w_list_x0 + 2;
        available_window_width = w_width - pos_x - 1;

        recalc_buffer();

        ui.position_from_window( w_con );
    } );
    ui.mark_resize();

    ui.on_redraw( [&]( ui_adaptor & ui ) {
        draw_grid( w_con, w_list_width + w_list_x0 );

        // Erase existing tab selection & list of constructions
        mvwhline( w_con, point::south_east, BORDER_COLOR, ' ', w_list_width );
        werase( w_list );
        // Print new tab listing
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        mvwprintz( w_con, point( 1, 1 ), c_yellow, "<< %s >>", construct_cat[tabindex].name() );
        // Determine where in the master list to start printing
        calcStartPos( offset, select, w_list_height, constructs.size() );
        // Print the constructions between offset and max (or how many will fit)
        for( size_t i = 0; static_cast<int>( i ) < w_list_height &&
             ( i + offset ) < constructs.size(); i++ ) {
            int current = i + offset;
            const construction_group_str_id &group = constructs[current];
            bool highlight = current == select;
            const point print_from( 0, i );
            if( highlight ) {
                ui.set_cursor( w_list, print_from );
            }
            trim_and_print( w_list, print_from, w_list_width,
                            construction_color( group, highlight ), group->name() );
        }

        // Clear out lines for tools & materials
        mvwrectf( w_con, point( pos_x, 1 ), BORDER_COLOR, ' ', available_window_width, w_height - 2 );

        // print the hotkeys regardless of if there are constructions
        for( size_t i = 0; i < notes.size(); ++i ) {
            trim_and_print( w_con, point( pos_x,
                                          w_height - 1 - static_cast<int>( notes.size() ) + static_cast<int>( i ) ),
                            available_window_width, c_white, notes[i] );
        }

        if( !constructs.empty() ) {
            if( select >= static_cast<int>( constructs.size() ) ) {
                select = 0;
            }
            const construction_group_str_id &current_group = constructs[select];
            // Print construction name
            trim_and_print( w_con, point( pos_x, 1 ), available_window_width, c_white, current_group->name() );

            if( current_construct_breakpoint > 0 ) {
                // Print previous stage indicator if breakpoint is past the beginning
                trim_and_print( w_con, point( pos_x, 2 ), available_window_width, c_white,
                                _( "Press [<color_yellow>%s</color>] to show previous stage(s)." ),
                                ctxt.get_desc( "SCROLL_STAGE_UP" ) );
            }
            if( static_cast<size_t>( construct_buffer_breakpoints[current_construct_breakpoint] ) +
                available_buffer_height < full_construct_buffer.size() ) {
                // Print next stage indicator if more breakpoints are remaining after screen height
                trim_and_print( w_con, point( pos_x, w_height - 2 - static_cast<int>( notes.size() ) ),
                                available_window_width, c_white,
                                _( "Press [<color_yellow>%s</color>] to show next stage(s)." ),
                                ctxt.get_desc( "SCROLL_STAGE_DOWN" ) );
            }
            // Leave room for above/below indicators
            int ypos = 3;
            nc_color stored_color = color_stage;
            for( size_t i = static_cast<size_t>( construct_buffer_breakpoints[current_construct_breakpoint] );
                 i < full_construct_buffer.size(); i++ ) {
                //the value of 3 is from leaving room at the top of window
                if( ypos > available_buffer_height + 3 ) {
                    break;
                }
                print_colored_text( w_con, point( w_list_width + w_list_x0 + 2, ypos++ ), stored_color, color_stage,
                                    full_construct_buffer[i] );
            }
        }

        draw_scrollbar( w_con, select, w_list_height, constructs.size(), point( 0, 3 ) );
        wnoutrefresh( w_con );

        wnoutrefresh( w_list );
    } );

    construction_group_str_id con_preview_group = construction_group_str_id::NULL_ID();
    std::map<tripoint_bub_ms, const construction *> con_preview;
    const std::optional<tripoint_bub_ms> mouse_pos; // dummy
    bool blink = true;
    shared_ptr_fast<game::draw_callback_t> draw_preview = construction_preview_callback(
                con_preview, mouse_pos, blink );
    g->add_draw_callback( draw_preview );

    do {
        if( update_cat ) {
            update_cat = false;
            construction_group_str_id last_construction = construction_group_str_id::NULL_ID();
            if( isnew ) {
                filter = uistate.construction_filter;
                filter.clear();
                tabindex = uistate.construction_tab.is_valid()
                           ? uistate.construction_tab.id().to_i() : 0;
                if( uistate.last_construction.is_valid() ) {
                    last_construction = uistate.last_construction;
                }
            } else if( select >= 0 && static_cast<size_t>( select ) < constructs.size() ) {
                last_construction = constructs[select];
            } else {
                filter.clear();
            }
            category_id = construct_cat[tabindex].id;
            if( category_id == construction_category_ALL ) {
                constructs = available;
            } else if( category_id == construction_category_FILTER ) {
                constructs.clear();
                std::copy_if( available.begin(), available.end(),
                              std::back_inserter( constructs ),
                [&]( const construction_group_str_id & group ) {
                    return lcmatch( group->name(), filter );
                } );
            } else {
                constructs = cat_available[category_id];
            }
            select = 0;
            if( last_construction ) {
                const auto it = std::find( constructs.begin(), constructs.end(),
                                           last_construction );
                if( it != constructs.end() ) {
                    select = std::distance( constructs.begin(), it );
                }
            }
        }
        isnew = false;
        static int lang_version = detail::get_current_language_version();

        //lang check here is needed to redraw the menu when using "Toggle language to English" option
        if( update_info || lang_version != detail::get_current_language_version() ) {
            update_info = false;
            lang_version = detail::get_current_language_version();

            notes.clear();
            if( tabindex == tabcount - 1 && !filter.empty() ) {
                notes.push_back( string_format( _( "Press [<color_red>%s</color>] to clear filter." ),
                                                ctxt.get_desc( "RESET_FILTER" ) ) );
            }
            notes.push_back( string_format( _( "Press [<color_yellow>%s or %s</color>] to tab." ),
                                            ctxt.get_desc( "LEFT" ),
                                            ctxt.get_desc( "RIGHT" ) ) );
            notes.push_back( string_format( _( "Press [<color_yellow>%s</color>] to search." ),
                                            ctxt.get_desc( "FILTER" ) ) );
            if( !hide_unconstructable ) {
                notes.push_back( string_format(
                                     _( "Press [<color_yellow>%s</color>] to hide unavailable constructions." ),
                                     ctxt.get_desc( "TOGGLE_UNAVAILABLE_CONSTRUCTIONS" ) ) );
            } else {
                notes.push_back( string_format(
                                     _( "Press [<color_red>%s</color>] to show unavailable constructions." ),
                                     ctxt.get_desc( "TOGGLE_UNAVAILABLE_CONSTRUCTIONS" ) ) );
            }
            notes.push_back( string_format(
                                 _( "Press [<color_yellow>%s</color>] to view and edit keybindings." ),
                                 ctxt.get_desc( "HELP_KEYBINDINGS" ) ) );

            recalc_buffer();
        } // Finished updating

        if( select < 0 || static_cast<size_t>( select ) >= constructs.size()
            || con_preview_group != constructs[select] ) {
            con_preview_group = ( select >= 0 && static_cast<size_t>( select ) < constructs.size() )
                                ? constructs[select] : construction_group_str_id::NULL_ID();
            if( con_preview_group.is_null() ) {
                con_preview.clear();
            } else {
                con_preview = valid_constructions_near_player(
                { con_preview_group }, total_inv, player_character ).first;
            }
        }

        g->invalidate_main_ui_adaptor();
        ui_manager::redraw();

        const std::string action = ctxt.handle_input();
        const int recmax = static_cast<int>( constructs.size() );
        const int scroll_rate = recmax > 20 ? 10 : 3;
        if( action == "TIMEOUT" ) {
            blink = !blink;
        } else {
            blink = true;
        }
        if( action == "FILTER" ) {
            string_input_popup popup;
            popup
            .title( _( "Search" ) )
            .width( 50 )
            .description( _( "Filter" ) )
            .max_length( 100 )
            .text( tabindex == tabcount - 1 ? filter : std::string() )
            .identifier( "construction" )
            .query_string();
            if( popup.confirmed() ) {
                filter = popup.text();
                uistate.construction_filter = filter;
                update_info = true;
                update_cat = true;
                tabindex = tabcount - 1;
            }
        } else if( action == "RESET_FILTER" ) {
            if( tabindex == tabcount - 1 && !filter.empty() ) {
                filter.clear();
                uistate.construction_filter.clear();
                update_info = true;
                update_cat = true;
            }
        } else if( navigate_ui_list( action, select, scroll_rate, recmax, true ) ) {
            update_info = true;
        } else if( action == "LEFT" || action == "PREV_TAB" || action == "RIGHT" || action == "NEXT_TAB" ) {
            update_info = true;
            update_cat = true;
            tabindex = inc_clamp_wrap( tabindex, action == "RIGHT" || action == "NEXT_TAB", tabcount );
        } else if( action == "SCROLL_STAGE_UP" || action == "SCROLL_STAGE_DOWN" ) {
            current_construct_breakpoint = inc_clamp( current_construct_breakpoint,
                                           action == "SCROLL_STAGE_DOWN", total_project_breakpoints - 1 );
        } else if( action == "QUIT" ) {
            exit = true;
        } else if( action == "TOGGLE_UNAVAILABLE_CONSTRUCTIONS" ) {
            update_info = true;
            update_cat = true;
            hide_unconstructable = !hide_unconstructable;
            offset = 0;
            load_available_constructions( available, cat_available, hide_unconstructable );
        } else if( action == "CONFIRM" ) {
            if( constructs.empty() || select >= static_cast<int>( constructs.size() ) ) {
                // Nothing to be done here
                continue;
            }
            if( !blueprint ) {
                if( player_can_build( player_character, total_inv, constructs[select] ) ) {
                    if( constructs[select] != construction_group_deconstruct_simple_furniture &&
                        !player_can_see_to_build( player_character, constructs[select] ) ) {
                        add_msg( m_info, _( "It is too dark to construct right now." ) );
                    } else if( !g->warn_player_maybe_anger_local_faction( true ) ) {
                        continue; // player declined to mess with faction's stuff
                    } else {
                        draw_preview.reset();
                        restore_view.reset();
                        restore_ui.reset();
                        ui.reset();
                        place_construction( { constructs[select] } );
                        uistate.last_construction = constructs[select];
                    }
                    exit = true;
                } else {
                    popup( _( "You can't build that!" ) );
                    update_info = true;
                }
            } else {
                // get the index of the overall constructions list from current_group
                const std::vector<construction> &list_constructions = get_constructions();
                for( int i = 0; i < static_cast<int>( list_constructions.size() ); ++i ) {
                    if( constructs[select] == list_constructions[i].group ) {
                        ret = construction_id( i );
                        break;
                    }
                }
                exit = true;
            }
        }
    } while( !exit );

    uistate.construction_tab = int_id<construction_category>( tabindex ).id();

    return ret;
}

static std::vector<construction *> player_can_build_valid_constructions( Character &you,
        const read_only_visitable &inv,
        const construction_group_str_id &group )
{
    std::vector<construction *> result;

    // check all with the same group to see if player can build any
    // if so, it will be added to the result
    std::vector<construction *> cons = constructions_by_group( group );
    for( construction *&con : cons ) {
        if( player_can_build( you, inv, *con ) ) {
            result.push_back( con );
        }
    }
    return result;
}

bool player_can_build( Character &you, const read_only_visitable &inv,
                       const construction_group_str_id &group )
{
    // check all with the same group to see if player can build any
    std::vector<construction *> cons = constructions_by_group( group );
    for( construction *&con : cons ) {
        if( player_can_build( you, inv, *con ) ) {
            return true;
        }
    }
    return false;
}

bool player_can_build( Character &you, const read_only_visitable &inv, const construction &con,
                       const bool can_construct_skip )
{
    if( you.has_trait( trait_DEBUG_HS ) ) {
        return true;
    }

    if( !you.meets_skill_requirements( con ) ) {
        return false;
    }

    // check for construction spot can be skipped by using can_construct_skip
    return con.requirements->can_make_with_inventory( inv, is_crafting_component, 1, craft_flags::none,
            false ) &&
           ( can_construct_skip || can_construct( con ) );
}

bool player_can_see_to_build( Character &you, const construction_group_str_id &group )
{
    if( you.fine_detail_vision_mod() < 4 || you.has_trait( trait_DEBUG_HS ) ) {
        return true;
    }
    std::vector<construction *> cons = constructions_by_group( group );
    for( construction *&con : cons ) {
        if( con->dark_craftable ) {
            return true;
        }
    }
    return false;
}

bool can_construct_furn_ter( const construction &con, furn_id const &f, ter_id const &t )
{
    return std::all_of( con.pre_flags.begin(), con.pre_flags.end(), [&f, &t]( auto const & flag ) {
        const bool use_ter = flag.second || f == furn_str_id::NULL_ID();
        return ( use_ter || f->has_flag( flag.first ) ) &&
               ( !use_ter || t->has_flag( flag.first ) );
    } );
}

bool can_construct( const construction &con, const tripoint_bub_ms &p )
{
    const map &here = get_map();
    const furn_id &f = here.furn( p );
    const ter_id &t = here.ter( p );
    if( con.pre_specials.size() > 1 ) { // pre-functions
        for( const auto &special : con.pre_specials ) {
            if( !special( p ) ) {
                return false;
            }
        }
    } else if( !con.pre_special( p ) ) { // pre-function
        return false;
    }
    if( !has_pre_terrain( con, p ) || // terrain type
        !can_construct_furn_ter( con, f, t ) ) { // flags
        return false;
    }
    if( !con.post_terrain.empty() ) { // make sure the construction would actually do something
        if( con.post_is_furniture ) {
            return f != furn_id( con.post_terrain );
        } else {
            return t != ter_id( con.post_terrain );
        }
    }
    return true;
}

bool can_construct( const construction &con )
{
    tripoint_bub_ms avatar_pos = get_player_character().pos_bub();
    for( const tripoint_bub_ms &p : get_map().points_in_radius( avatar_pos, 1 ) ) {
        if( p != avatar_pos && can_construct( con, p ) ) {
            return true;
        }
    }
    return false;
}

std::pair<std::map<tripoint_bub_ms, const construction *>, std::vector<construction *>>
        valid_constructions_near_player( const std::vector<construction_group_str_id> &groups,
                const inventory &total_inv, avatar &player_character )
{
    std::pair<std::map<tripoint_bub_ms, const construction *>, std::vector<construction *>> ret;
    std::map<tripoint_bub_ms, const construction *> &valid = ret.first;
    std::vector<construction *> &cons = ret.second;
    map &here = get_map();
    for( construction_group_str_id const &group : groups ) {
        std::vector<construction *> const temp = constructions_by_group( group );
        for( const tripoint_bub_ms &p : here.points_in_radius( player_character.pos_bub(), 1 ) ) {
            for( const auto *con : temp ) {
                if( p != player_character.pos_bub() && can_construct( *con, p ) &&
                    player_can_build( player_character, total_inv, *con, true ) ) {
                    valid[ p ] = con;
                }
            }
        }
        std::move( temp.begin(), temp.end(), std::back_inserter( cons ) );
    }
    return ret;
}

void place_construction( std::vector<construction_group_str_id> const &groups )
{
    avatar &player_character = get_avatar();
    const inventory &total_inv = player_character.crafting_inventory();

    std::pair<std::map<tripoint_bub_ms, const construction *>, std::vector<construction *>>
            valid_pair = valid_constructions_near_player( groups, total_inv, player_character );
    std::map<tripoint_bub_ms, const construction *> &valid = valid_pair.first;
    std::vector<construction *> &cons = valid_pair.second;
    map &here = get_map();

    bool blink = true;
    std::optional<tripoint_bub_ms> mouse_pos;

#if defined( TILES )
    on_out_of_scope reenable_occlusion( []() {
        tilecontext->set_disable_occlusion( false );
        g->invalidate_main_ui_adaptor();
    } );
    tilecontext->set_disable_occlusion( true );
    g->invalidate_main_ui_adaptor();
#endif

    shared_ptr_fast<game::draw_callback_t> draw_preview = construction_preview_callback(
                valid, mouse_pos, blink );
    g->add_draw_callback( draw_preview );

    const tripoint_bub_ms &loc = player_character.pos_bub();
    const std::optional<tripoint_bub_ms> pnt_ = choose_adjacent(
                loc, _( "Construct where?" ),
                /*allow_vertical=*/false, /*timeout=*/get_option<int>( "BLINK_SPEED" ),
    [&]( const input_context & ctxt, const std::string & action ) {
        if( action == "TIMEOUT" ) {
            blink = !blink;
        } else {
            blink = true;
        }
        if( action == "MOUSE_MOVE" ) {
            const std::optional<tripoint_bub_ms> mouse_pos_raw = ctxt.get_coordinates(
                        g->w_terrain, g->ter_view_p.raw().xy(), true );
            if( mouse_pos_raw.has_value() && mouse_pos_raw->z() == loc.z()
                && mouse_pos_raw->x() >= loc.x() - 1 && mouse_pos_raw->x() <= loc.x() + 1
                && mouse_pos_raw->y() >= loc.y() - 1 && mouse_pos_raw->y() <= loc.y() + 1 ) {
                mouse_pos = *mouse_pos_raw;
            } else {
                mouse_pos = std::nullopt;
            }
        }
        g->invalidate_main_ui_adaptor();
        return std::pair<bool, std::optional<tripoint_bub_ms>>( false, std::nullopt );
    } );
    if( !pnt_ ) {
        return;
    }
    const tripoint_bub_ms &pnt = *pnt_;

    if( valid.find( pnt ) == valid.end() ) {
        cons.front()->explain_failure( pnt );
        return;
    }
    // Maybe there is already a partial_con on an existing trap, that isn't caught by the usual trap-checking.
    // because the pre-requisite construction is already a trap anyway.
    // This shouldn't normally happen, unless it's a spike pit being built on a pit for example.
    partial_con *pre_c = here.partial_con_at( pnt );
    if( pre_c ) {
        add_msg( m_info,
                 _( "There is already an unfinished construction there, examine it to continue working on it." ) );
        return;
    }
    std::list<item> used;
    const construction &con = *valid.find( pnt )->second;
    // create the partial construction struct
    partial_con pc;
    pc.id = con.id;
    if( player_character.has_trait( trait_DEBUG_HS ) ) {
        // Gift components
        for( const auto &it : con.requirements->get_components() ) {
            used.emplace_back( it.front().type );
        }
    } else {
        // Use up the components
        for( const std::vector<item_comp> &it : con.requirements->get_components() ) {
            std::list<item> tmp = player_character.consume_items( it, 1, is_crafting_component );
            used.splice( used.end(), tmp );
        }
    }
    pc.components = used;
    here.partial_con_set( pnt, pc );
    for( const auto &it : con.requirements->get_tools() ) {
        player_character.consume_tools( it );
    }
    player_character.invalidate_crafting_inventory();
    player_character.invalidate_weight_carried_cache();
    player_character.assign_activity( ACT_BUILD );
    player_character.activity.placement = here.get_abs( pnt );
}

void complete_construction( Character *you )
{
    if( !finalized ) {
        debugmsg( "complete_construction called before finalization" );
        return;
    }
    map &here = get_map();
    const tripoint_bub_ms terp = here.get_bub( you->activity.placement );
    partial_con *pc = here.partial_con_at( terp );
    if( !pc ) {
        debugmsg( "No partial construction found at activity placement in complete_construction()" );
        if( you->is_npc() ) {
            npc *guy = dynamic_cast<npc *>( you );
            guy->current_activity_id = activity_id::NULL_ID();
            guy->revert_after_activity();
            guy->set_moves( 0 );
        }
        return;
    }
    const construction &built = pc->id.obj();
    you->activity.str_values.emplace_back( built.str_id );
    const auto award_xp = [&]( Character & practicer ) {
        for( const auto &pr : built.required_skills ) {
            practicer.practice( pr.first, static_cast<int>( ( 10 + 15 * pr.second ) *
                                ( 1 + built.time / 180000.0 ) ),
                                static_cast<int>( pr.second * 1.25 ) );
        }
    };

    award_xp( *you );
    // Other friendly Characters gain exp from assisting or watching...
    // TODO: Characters watching other Characters do stuff and learning from it
    if( you->is_avatar() ) {
        for( Character *elem : get_avatar().get_crafting_helpers() ) {
            if( elem->meets_skill_requirements( built ) ) {
                add_msg( m_info, _( "%s assists you with the work…" ), elem->get_name() );
            } else {
                //NPC near you isn't skilled enough to help
                add_msg( m_info, _( "%s watches you work…" ), elem->get_name() );
            }

            award_xp( *elem );
        }
    }

    // partial_con contains components for vehicle and appliance construction
    // it's removal is handled in done_appliance() / done_vehicle
    if( pc->id->category != construction_category_APPLIANCE &&
        pc->id->str_id != construction_constr_veh ) {
        here.partial_con_remove( terp );
    }

    // Some constructions are allowed to have items left on the tile.
    if( built.post_flags.count( "keep_items" ) == 0 ) {
        // Move any items that have found their way onto the construction site.
        std::vector<tripoint_bub_ms> dump_spots;
        for( const tripoint_bub_ms &pt : here.points_in_radius( terp, 1 ) ) {
            if( here.can_put_items( pt ) && pt != terp ) {
                dump_spots.push_back( pt );
            }
        }
        if( !dump_spots.empty() ) {
            tripoint_bub_ms dump_spot = random_entry( dump_spots );
            map_stack items = here.i_at( terp );
            for( map_stack::iterator it = items.begin(); it != items.end(); ) {
                here.add_item_or_charges( dump_spot, *it );
                it = items.erase( it );
            }
        } else {
            debugmsg( "No space to displace items from construction finishing" );
        }
    }
    // Make the terrain change
    if( !built.post_terrain.empty() ) {
        if( built.post_is_furniture ) {
            here.furn_set( terp, furn_str_id( built.post_terrain ) );
        } else {
            here.ter_set( terp, ter_str_id( built.post_terrain ) );
        }
    }

    // Spawn byproducts
    if( built.byproduct_item_group ) {
        here.spawn_items( you->pos_bub(), item_group::items_from( *built.byproduct_item_group,
                          calendar::turn ) );
    }

    add_msg( m_info, _( "%s finished construction: %s." ), you->disp_name( false, true ),
             built.group->name() );
    // clear the activity
    you->activity.set_to_null();
    you->recoil = MAX_RECOIL;

    // This comes after clearing the activity, in case the function interrupts
    // activities
    if( built.post_specials.size() > 1 ) { // pre-functions
        for( const auto &special : built.post_specials ) {
            special( terp, *you );
        }
    } else {
        built.post_special( terp, *you );
    }
    // Players will not automatically resume backlog, other Characters will.
    if( you->is_avatar() && !you->backlog.empty() &&
        you->backlog.front().id() == ACT_MULTIPLE_CONSTRUCTION ) {
        you->backlog.clear();
        you->assign_activity( ACT_MULTIPLE_CONSTRUCTION );
    }
}

bool construct::check_channel( const tripoint_bub_ms &p )
{
    map &here = get_map();
    const std::function<bool( const point & )> has_current = [&p, &here]( const point & offset ) {
        return here.has_flag( ter_furn_flag::TFLAG_CURRENT, p + offset );
    };
    const std::function<bool( const tripoint_abs_omt & )> river_at = []( const tripoint_abs_omt & pt ) {
        return is_river( overmap_buffer.ter( pt ) );
    };
    if( !std::any_of( four_adjacent_offsets.begin(), four_adjacent_offsets.end(), has_current ) ) {
        return false;
    }
    tripoint_abs_omt omt_pt = project_to<coords::omt>( here.get_abs( p ) );
    tripoint_range<tripoint_abs_omt> nearby_omts = points_in_radius<tripoint_abs_omt>( omt_pt, 1 );
    if( !std::any_of( nearby_omts.begin(), nearby_omts.end(), river_at ) ) {
        return false;
    }
    return true;
}

bool construct::check_empty_lite( const tripoint_bub_ms &p )
{
    map &here = get_map();
    return ( !here.has_furn( p ) && g->is_empty( p ) && here.tr_at( p ).is_null() &&
             here.i_at( p ).empty() && !here.veh_at( p ) );
}

bool construct::check_empty( const tripoint_bub_ms &p )
{
    map &here = get_map();
    // @TODO should check for *visible* traps only. But calling code must
    // first know how to handle constructing on top of an invisible trap!
    return here.has_flag( ter_furn_flag::TFLAG_FLAT, p ) && !here.has_furn( p ) &&
           g->is_empty( p ) && here.tr_at( p ).is_null() &&
           here.i_at( p ).empty() && !here.veh_at( p );
}

bool construct::check_unblocked( const tripoint_bub_ms &p )
{
    map &here = get_map();
    // @TODO should check for *visible* traps only. But calling code must
    // first know how to handle constructing on top of an invisible trap!
    // Should also check for empty space rather than open air, when such a check exists.
    return !here.has_furn( p ) &&
           ( g->is_empty( p ) || here.ter( p ) == ter_t_open_air ) && here.tr_at( p ).is_null() &&
           here.i_at( p ).empty() && !here.veh_at( p );
}

bool construct::check_support( const tripoint_bub_ms &p )
{
    map &here = get_map();
    // need two or more orthogonally adjacent supports
    if( here.impassable( p ) ) {
        return false;
    }

    // The current collapse logic is based on the level below supporting a "roof" rather
    // than the orthogonal tiles supporting a roof tile. Thus, the construction logic
    // uses similar criteria, which means you may have to first change grass into a dirt
    // floor before you may be able to build a roof above it.
    int num_supports = 0;
    for( const point &offset : four_adjacent_offsets ) {
        if( here.has_flag( ter_furn_flag::TFLAG_SUPPORTS_ROOF, p + offset ) ||
            ( !here.ter( p + offset )->has_flag( "EMPTY_SPACE" ) &&
              here.has_flag( ter_furn_flag::TFLAG_SUPPORTS_ROOF, p + offset + tripoint::below ) ) ) {
            num_supports++;
        }
    }
    // We want to find "walls" below (including windows and doors), but not open rooms and the like.
    if( here.has_flag( ter_furn_flag::TFLAG_SUPPORTS_ROOF, p + tripoint::below ) &&
        ( here.has_flag( ter_furn_flag::TFLAG_WALL, p + tripoint::below ) ||
          here.has_flag( ter_furn_flag::TFLAG_DOOR, p + tripoint::below ) ||
          here.has_flag( ter_furn_flag::TFLAG_WINDOW, p + tripoint::below ) ) ) {
        num_supports += 2;
    }

    return num_supports >= 2;
}

bool construct::check_support_below( const tripoint_bub_ms &p )
{
    bool blocking_creature = g->get_creature_if( [&]( const Creature & creature ) {
        return creature.pos_bub() == p;
    } ) != nullptr;

    map &here = get_map();
    // These checks are based on check_empty_lite, but accept no floor and the traps associated
    // with it, i.e. ledge, and various forms of pits.
    // - Check if there's nothing in the way. The "passable" check rejects tiles you can't
    //   pass through, but we want air and water to be OK as well (that's what we want to bridge).
    // - Apart from that, vehicles, items, creatures, traps and furniture also have to be absent.
    if( !( here.passable( p ) || here.has_flag( ter_furn_flag::TFLAG_LIQUID, p ) ||
           here.has_flag( ter_furn_flag::TFLAG_NO_FLOOR, p ) ) || blocking_creature || here.has_furn( p ) ||
        !here.tr_at( p ).is_null() || !here.i_at( p ).empty() || here.veh_at( p ) ) {
        return false;
    }
    // need two or more orthogonally adjacent supports at the Z level below
    int num_supports = 0;
    for( const point &offset : four_adjacent_offsets ) {
        if( here.has_flag( ter_furn_flag::TFLAG_SUPPORTS_ROOF, p + offset + tripoint::below ) &&
            !here.has_flag( ter_furn_flag::TFLAG_SINGLE_SUPPORT, p + offset + tripoint::below ) ) {
            num_supports++;
        }
    }
    // We want to find "walls" below (including windows and doors), but not open rooms and the like.
    if( here.has_flag( ter_furn_flag::TFLAG_SUPPORTS_ROOF, p + tripoint::below ) &&
        ( here.has_flag( ter_furn_flag::TFLAG_WALL, p + tripoint::below ) ||
          here.has_flag( ter_furn_flag::TFLAG_CONNECT_WITH_WALL, p + tripoint::below ) ) ) {
        num_supports += 2;
    }
    return num_supports >= 2;
}

bool construct::check_single_support( const tripoint_bub_ms &p )
{
    map &here = get_map();
    if( here.impassable( p ) ) {
        return false;
    }
    return here.has_flag( ter_furn_flag::TFLAG_SUPPORTS_ROOF, p + tripoint::below );
}

bool construct::check_stable( const tripoint_bub_ms &p )
{
    return get_map().has_flag( ter_furn_flag::TFLAG_SUPPORTS_ROOF, p + tripoint::below );
}

bool construct::check_nofloor_above( const tripoint_bub_ms &p )
{
    return get_map().has_flag( ter_furn_flag::TFLAG_NO_FLOOR, p + tripoint::above );
}

bool construct::check_deconstruct( const tripoint_bub_ms &p )
{
    map &here = get_map();
    if( here.has_furn( p ) ) {
        // Can deconstruct furniture here, make sure regular deconstruction isn't available if easy deconstruction is possible
        if( here.has_flag_furn( ter_furn_flag::TFLAG_EASY_DECONSTRUCT, p ) ) {
            return false;
        }
        return !!here.furn( p ).obj().deconstruct;
    }
    // terrain can only be deconstructed when there is no furniture in the way
    return !!here.ter( p ).obj().deconstruct;
}

bool construct::check_up_OK( const tripoint_bub_ms & )
{
    // You're not going above +OVERMAP_HEIGHT.
    return ( get_map().get_abs_sub().z() < OVERMAP_HEIGHT );
}

bool construct::check_down_OK( const tripoint_bub_ms & )
{
    // You're not going below -OVERMAP_DEPTH.
    return ( get_map().get_abs_sub().z() > -OVERMAP_DEPTH );
}

bool construct::check_no_trap( const tripoint_bub_ms &p )
{
    return get_map().tr_at( p ).is_null();
}

bool construct::check_ramp_high( const tripoint_bub_ms &p )
{
    map &here = get_map();
    for( const point &offset : four_adjacent_offsets ) {
        if( here.has_flag( ter_furn_flag::TFLAG_RAMP_DOWN, p + offset + tripoint::above ) ) {
            return true;
        }
    }
    return false;
}

bool construct::check_no_wiring( const tripoint_bub_ms &p )
{
    const optional_vpart_position vp = get_map().veh_at( p );
    if( !vp ) {
        return true;
    }

    const vehicle &veh_target = vp->vehicle();
    return !veh_target.has_tag( flag_WIRING );
}

bool construct::check_matching_down_above( const tripoint_bub_ms &p )
{
    map &here = get_map();
    const std::string ter_here = here.ter( p ).id().str();
    const std::string ter_above = here.ter( p + tripoint::above ).id().str();
    const size_t separation = ter_here.find_last_of( '_' );
    return separation > 0 &&
           ter_here.substr( 0, separation + 1 ) + "down" == ter_above;
}

void construct::done_trunk_plank( const tripoint_bub_ms &/*p*/, Character &/*who*/ )
{
    int num_logs = rng( 2, 3 );
    Character &player_character = get_player_character();
    for( int i = 0; i < num_logs; ++i ) {
        iuse::cut_log_into_planks( player_character );
    }
}

void construct::done_grave( const tripoint_bub_ms &p, Character &player_character )
{
    map &here = get_map();
    map_stack its = here.i_at( p );
    for( const item &it : its ) {
        if( it.is_corpse() ) {
            if( it.get_corpse_name().empty() ) {
                if( it.get_mtype()->has_flag( mon_flag_HUMAN ) ) {
                    if( player_character.has_trait( trait_SPIRITUAL ) ) {
                        player_character.add_morale( morale_funeral, 50, 75, 1_days, 1_hours );
                        add_msg( m_good,
                                 _( "You feel relieved after providing last rites for this human being, whose name is lost in the Cataclysm." ) );
                    } else {
                        add_msg( m_neutral, _( "You bury remains of a human, whose name is lost in the Cataclysm." ) );
                    }
                }
            } else {
                if( player_character.has_trait( trait_SPIRITUAL ) ) {
                    player_character.add_morale( morale_funeral, 50, 75, 1_days, 1_hours );
                    add_msg( m_good,
                             _( "You feel sadness, but also relief after providing last rites for %s, whose name you will keep in your memory." ),
                             it.get_corpse_name() );
                } else {
                    add_msg( m_neutral,
                             _( "You bury remains of %s, who joined uncounted masses perished in the Cataclysm." ),
                             it.get_corpse_name() );
                }
            }
            get_event_bus().send<event_type::buries_corpse>(
                player_character.getID(), it.get_mtype()->id, it.get_corpse_name() );
        }
    }
    if( player_character.has_quality( qual_CUT ) ) {
        iuse::handle_ground_graffiti( player_character, nullptr, _( "Inscribe something on the grave?" ),
                                      &here, p );
    } else {
        add_msg( m_neutral,
                 _( "Unfortunately you don't have anything sharp to place an inscription on the grave." ) );
    }

    here.destroy_furn( p, true );
}

static vpart_id vpart_from_item( const itype_id &item_id )
{
    for( const vpart_info &vpi : vehicles::parts::get_all() ) {
        if( vpi.base_item == item_id && vpi.has_flag( flag_INITIAL_PART ) ) {
            return vpi.id;
        }
    }
    // The INITIAL_PART flag is optional, if no part (based on the given item) has it, just use the
    // first part that is based in the given item (this is fine for example if there is only one
    // such type anyway).
    for( const vpart_info &vpi : vehicles::parts::get_all() ) {
        if( vpi.base_item == item_id ) {
            return vpi.id;
        }
    }
    debugmsg( "item %s used by construction is not base item of any vehicle part!", item_id.c_str() );
    return vpart_frame;
}

void construct::done_vehicle( const tripoint_bub_ms &p, Character & )
{
    std::string name = string_input_popup()
                       .title( _( "Enter new vehicle name:" ) )
                       .width( 20 )
                       .query_string();
    if( name.empty() ) {
        name = _( "Car" );
    }

    map &here = get_map();
    partial_con *pc = here.partial_con_at( p );
    if( !pc ) {
        debugmsg( "constructing failed: can't find partial construction" );
        return;
    }

    const std::list<item> components = pc->components;
    here.partial_con_remove( p );

    if( components.size() != 1 ) {
        debugmsg( "constructing failed: components size expected 1 actual %d", components.size() );
        return;
    }

    vehicle *veh = here.add_vehicle( vehicle_prototype_none, p, 270_degrees, 0, 0 );

    if( !veh ) {
        debugmsg( "constructing failed: add_vehicle returned null" );
        return;
    }
    const item &base = components.front();

    veh->name = name;
    const int partnum = veh->install_part( here, point_rel_ms::zero, vpart_from_item( base.typeId() ),
                                           item( base ) );
    veh->part( partnum ).set_flag( vp_flag::unsalvageable_flag );

    // Update the vehicle cache immediately,
    // or the vehicle will be invisible for the first couple of turns.
    here.add_vehicle_to_cache( veh );
}

void construct::done_wiring( const tripoint_bub_ms &p, Character &who )
{
    map &here = get_map();
    here.partial_con_remove( p );

    place_appliance( here, p, vpart_from_item( itype_wall_wiring ), who );
    if( who.is_avatar() && query_yn( _( "Also reveal all nearby wirings?" ) ) ) {
        // This is a *really* terrible check to ensure we iterate the current OMT and nothing else.
        const tripoint_abs_omt current_omt( coords::project_to<coords::omt>( here.get_abs( p ) ) );
        for( const tripoint_bub_ms &target : here.points_on_zlevel() ) {
            const tripoint_abs_omt target_omt( coords::project_to<coords::omt>( here.get_abs( target ) ) );
            if( target_omt != current_omt ) {
                continue;
            }
            if( here.has_flag_ter( ter_furn_flag::TFLAG_WIRED_WALL, target ) && check_no_wiring( target ) ) {
                place_appliance( here, target, vpart_from_item( itype_wall_wiring ), who );
            }
        }
    }
}

void construct::done_appliance( const tripoint_bub_ms &p, Character &who )
{
    map &here = get_map();

    partial_con *pc = here.partial_con_at( p );
    if( !pc ) {
        debugmsg( "constructing failed: can't find partial construction" );
        return;
    }

    const std::list<item> components = pc->components;
    here.partial_con_remove( p );

    if( components.size() != 1 ) {
        debugmsg( "constructing failed: components size expected 1 actual %d", components.size() );
        return;
    }

    const item &base = components.front();
    const vpart_id &vpart = vpart_appliance_from_item( base.typeId() );

    place_appliance( here, p, vpart, who, base );
}

void construct::done_deconstruct( const tripoint_bub_ms &p, Character &player_character )
{
    map &here = get_map();

    auto deconstruction_practice_skill = [ &player_character ]( auto & skill ) {
        if( player_character.get_skill_level( skill.id ) >= skill.min ) {
            // Uses a modified version of the complete_construction formula using 20 minutes with halved yield before the multiplier
            player_character.practice( skill.id,
                                       static_cast<int>( skill.multiplier * ( 5.0 / 6.0 ) * ( 10 + 7.5 * ( skill.min +
                                               skill.max ) ) ), skill.max );
        }
    };

    // TODO: Make this the argument
    if( here.has_furn( p ) ) {
        const furn_t &f = here.furn( p ).obj();
        if( !f.deconstruct ) {
            add_msg( m_info, _( "That %s can not be disassembled!" ), f.name() );
            return;
        }
        if( f.deconstruct->furn_set.str().empty() ) {
            here.furn_set( p, furn_str_id::NULL_ID() );
        } else {
            here.furn_set( p, f.deconstruct->furn_set );
        }
        add_msg( _( "The %s is disassembled." ), f.name() );
        item &item_here = here.i_at( p ).size() != 1 ? null_item_reference() : here.i_at( p ).only_item();
        const std::vector<item *> drop = here.spawn_items( p,
                                         item_group::items_from( f.deconstruct->drop_group, calendar::turn ) );
        if( f.deconstruct->skill.has_value() ) {
            deconstruction_practice_skill( f.deconstruct->skill.value() );
        }
        // if furniture has liquid in it and deconstructs into watertight containers then fill them
        if( f.has_flag( "LIQUIDCONT" ) && item_here.made_of( phase_id::LIQUID ) ) {
            for( item *it : drop ) {
                if( it->get_remaining_capacity_for_liquid( item_here ) <= 0 ) {
                    continue;
                }
                item_here.charges -= it->fill_with( item_here, item_here.charges, false, true, true );
                if( item_here.charges <= 0 ) {
                    here.i_rem( p, &item_here );
                    break;
                }
            }
        }
        // HACK: Hack alert.
        // Signs have cosmetics associated with them on the submap since
        // furniture can't store dynamic data to disk. To prevent writing
        // mysteriously appearing for a sign later built here, remove the
        // writing from the submap.
        here.delete_signage( p );
    } else {
        const ter_t &t = here.ter( p ).obj();
        if( !t.deconstruct ) {
            add_msg( _( "That %s can not be disassembled!" ), t.name() );
            return;
        }
        if( t.deconstruct->deconstruct_above ) {
            const tripoint_bub_ms top = p + tripoint::above;
            if( here.has_furn( top ) ) {
                add_msg( _( "That %s can not be disassembled, since there is furniture above it." ), t.name() );
                return;
            }
            done_deconstruct( top, player_character );
        }
        here.ter_set( p, t.deconstruct->ter_set );
        add_msg( _( "The %s is disassembled." ), t.name() );
        here.spawn_items( p, item_group::items_from( t.deconstruct->drop_group, calendar::turn ) );
        if( t.deconstruct->skill.has_value() ) {
            deconstruction_practice_skill( t.deconstruct->skill.value() );
        }
    }
}

static void unroll_digging( const int numer_of_2x4s )
{
    // refund components!
    item rope( itype_rope_30 );
    map &here = get_map();
    tripoint_bub_ms avatar_pos = get_player_character().pos_bub();
    here.add_item_or_charges( avatar_pos, rope );
    // presuming 2x4 to conserve lumber.
    here.spawn_item( avatar_pos, itype_2x4, numer_of_2x4s );
}

void construct::done_digormine_stair( const tripoint_bub_ms &p, bool dig,
                                      Character &player_character )
{
    map &here = get_map();
    const tripoint_bub_ms p_below = p + tripoint::below;

    bool dig_muts = player_character.has_trait( trait_PAINRESIST_TROGLO ) ||
                    player_character.has_trait( trait_STOCKY_TROGLO );

    int no_mut_penalty = dig_muts ? 10 : 0;
    int mine_penalty = dig ? 0 : 10;
    player_character.mod_stored_kcal( -43 - 9 * mine_penalty - 9 * no_mut_penalty );
    player_character.mod_thirst( 5 + mine_penalty + no_mut_penalty );
    player_character.mod_sleepiness( 10 + mine_penalty + no_mut_penalty );

    if( here.ter( p_below ) == ter_t_lava ) {
        if( !query_yn( _( "The rock feels much warmer than normal.  Proceed?" ) ) ) {
            here.ter_set( p, ter_t_pit ); // You dug down a bit before detecting the problem
            unroll_digging( dig ? 8 : 12 );
        } else {
            add_msg( m_warning, _( "You just tunneled into lava!" ) );
            get_event_bus().send<event_type::digs_into_lava>();
            here.ter_set( p, ter_t_open_air );
        }

        return;
    }

    bool impassable = here.impassable( p_below );
    if( !impassable ) {
        add_msg( _( "You dig into a preexisting space, and improvise a ladder." ) );
    } else if( dig ) {
        add_msg( _( "You dig a stairway, adding sturdy timbers and a rope for safety." ) );
    } else {
        add_msg( _( "You drill out a passage, heading deeper underground." ) );
    }
    here.ter_set( p, ter_t_stairs_down ); // There's the top half
    here.ter_set( p_below, impassable ? ter_t_stairs_up :
                  ter_t_ladder_up ); // and there's the bottom half.
}

void construct::done_dig_grave( const tripoint_bub_ms &p, Character &who )
{
    map &here = get_map();
    if( one_in( 10 ) ) {
        static const std::array<mtype_id, 5> monids = {
            { mon_zombie, mon_zombie_fat, mon_zombie_rot, mon_skeleton, mon_zombie_crawler }
        };

        g->place_critter_at( random_entry( monids ), p );
        here.furn_set( p, furn_f_coffin_o );
        who.add_msg_if_player( m_warning, _( "Something crawls out of the coffin!" ) );
    } else {
        here.spawn_item( p, itype_bone_human, rng( 5, 15 ) );
        here.furn_set( p, furn_f_coffin_c );
    }
    std::vector<item *> dropped =
        here.place_items( Item_spawn_data_allclothes, 50, p, p, false, calendar::turn );
    here.place_items( Item_spawn_data_grave, 25, p, p, false, calendar::turn );
    here.place_items( Item_spawn_data_jewelry_front, 20, p, p, false, calendar::turn );
    for( item * const &it : dropped ) {
        if( it->is_armor() ) {
            it->set_flag( json_flag_FILTHY );
            it->set_damage( rng( 1, it->max_damage() - 1 ) );
            it->rand_degradation();
        }
    }
    get_event_bus().send<event_type::exhumes_grave>( who.getID() );
}

void construct::done_dig_grave_nospawn( const tripoint_bub_ms &p, Character &who )
{
    get_map().furn_set( p, furn_f_coffin_c );
    get_event_bus().send<event_type::exhumes_grave>( who.getID() );
}

void construct::done_dig_stair( const tripoint_bub_ms &p, Character &who )
{
    done_digormine_stair( p, true, who );
}

void construct::done_mine_downstair( const tripoint_bub_ms &p, Character &who )
{
    done_digormine_stair( p, false, who );
}

void construct::done_mine_upstair( const tripoint_bub_ms &p, Character &player_character )
{
    map &here = get_map();
    const tripoint_bub_ms p_above = p + tripoint::above;

    if( here.ter( p_above ) == ter_t_lava ) {
        here.ter_set( p, ter_t_rock_floor ); // You dug a bit before discovering the problem
        add_msg( m_warning, _( "The rock overhead feels hot.  You decide *not* to mine magma." ) );
        unroll_digging( 12 );
        return;
    }

    if( here.has_flag_ter( ter_furn_flag::TFLAG_SHALLOW_WATER, p_above ) ||
        here.has_flag_ter( ter_furn_flag::TFLAG_DEEP_WATER, p_above ) ) {
        here.ter_set( p, ter_t_rock_floor ); // You dug a bit before discovering the problem
        add_msg( m_warning, _( "The rock above is rather damp.  You decide *not* to mine water." ) );
        unroll_digging( 12 );
        return;
    }

    bool dig_muts = player_character.has_trait( trait_PAINRESIST_TROGLO ) ||
                    player_character.has_trait( trait_STOCKY_TROGLO );

    int no_mut_penalty = dig_muts ? 15 : 0;
    player_character.mod_stored_kcal( -174 - 9 * no_mut_penalty );
    player_character.mod_thirst( 20 + no_mut_penalty );
    player_character.mod_sleepiness( 25 + no_mut_penalty );

    add_msg( _( "You drill out a passage, heading for the surface." ) );
    here.ter_set( p, ter_t_stairs_up ); // There's the bottom half
    here.ter_set( p_above, ter_t_stairs_down ); // and there's the top half.
}

void construct::done_wood_stairs( const tripoint_bub_ms &p, Character &/*who*/ )
{
    const tripoint_bub_ms top = p + tripoint::above;
    get_map().ter_set( top, ter_t_wood_stairs_down );
}

void construct::done_window_curtains( const tripoint_bub_ms &, Character &who )
{
    map &here = get_map();
    tripoint_bub_ms avatar_pos = who.pos_bub();
    // copied from iexamine::curtains
    here.spawn_item( avatar_pos, itype_nail, 1, 4 );
    here.spawn_item( avatar_pos, itype_sheet, 2 );
    here.spawn_item( avatar_pos, itype_stick );
    here.spawn_item( avatar_pos, itype_string_36 );
    add_msg( _( "After boarding up the window the curtains and curtain rod are left." ) );
}

void construct::done_extract_maybe_revert_to_dirt( const tripoint_bub_ms &p, Character &/*who*/ )
{
    map &here = get_map();
    if( one_in( 10 ) ) {
        here.ter_set( p, ter_t_dirt );
    }

    const ter_id &t = here.ter( p );
    if( t == ter_t_clay ) {
        add_msg( _( "You gather some clay." ) );
    } else if( t == ter_t_sand ) {
        add_msg( _( "You gather some sand." ) );
    } else {
        // Fall through to an undefined material.
        add_msg( _( "You gather some materials." ) );
    }
}

void construct::done_mark_firewood( const tripoint_bub_ms &p, Character &/*who*/ )
{
    get_map().trap_set( p, tr_firewood_source );
}

void construct::done_mark_practice_target( const tripoint_bub_ms &p, Character &/*who*/ )
{
    get_map().trap_set( p, tr_practice_target );
}

void construct::done_ramp_low( const tripoint_bub_ms &p, Character &/*who*/ )
{
    const tripoint_bub_ms top = p + tripoint::above;
    get_map().ter_set( top, ter_t_ramp_down_low );
}

void construct::done_ramp_high( const tripoint_bub_ms &p, Character &/*who*/ )
{
    const tripoint_bub_ms top = p + tripoint::above;
    get_map().ter_set( top, ter_t_ramp_down_high );
}

void construct::add_matching_down_above( const tripoint_bub_ms &p, Character &/*who*/ )
{
    map &here = get_map();
    const std::string ter_here = here.ter( p ).id().str();
    const std::string ter_above = here.ter( p + tripoint::above ).id().str();
    const size_t separation = ter_here.find_last_of( '_' );
    if( separation > 0 ) {
        here.ter_set( p + tripoint::above, ter_id( ter_here.substr( 0, separation + 1 ) + "down" ) );
    }
}

void construct::remove_above( const tripoint_bub_ms &p, Character &/*who*/ )
{
    map &here = get_map();
    here.ter_set( p + tripoint::above, ter_t_open_air );
}

void construct::add_roof( const tripoint_bub_ms &p, Character &/*who*/ )
{
    map &here = get_map();
    const ter_id &t = here.ter( p );
    const ter_id &roof = t.obj().roof;
    if( !roof ) {
        debugmsg( "add_roof post_ter called on terrain lacking roof definition, %s.",
                  t.id().c_str() );
    }
    here.ter_set( p + tripoint::above, roof );
}

void construct::do_turn_deconstruct( const tripoint_bub_ms &p, Character &who )
{
    map &here = get_map();
    // Only run once at the start of construction
    if( here.partial_con_at( p )->counter == 0 && who.is_avatar() &&
        get_option<bool>( "QUERY_DECONSTRUCT" ) ) {
        bool cancel_construction = false;

        auto deconstruct_query = [&]( const std::string & info ) {
            cancel_construction = !who.query_yn( string_format( _( "%s\nConfirm deconstruct?" ), info ) );
        };

        std::string tname;
        if( here.has_furn( p ) ) {
            const furn_t &f = here.furn( p ).obj();
            if( f.deconstruct ) {
                deconstruct_query( f.deconstruct->potential_deconstruct_items( f.name() ) );
            }
        } else {
            const ter_t &t = here.ter( p ).obj();
            if( t.deconstruct ) {
                deconstruct_query( t.deconstruct->potential_deconstruct_items( t.name() ) );
            }
        }
        if( cancel_construction ) {
            here.partial_con_remove( p );
            who.cancel_activity();
        }
    }
}

void construct::do_turn_shovel( const tripoint_bub_ms &p, Character &who )
{
    sfx::play_activity_sound( "tool", "shovel", sfx::get_heard_volume( p ) );
    if( calendar::once_every( 1_minutes ) ) {
        //~ Sound of a shovel digging a pit at work!
        sounds::sound( p, 10, sounds::sound_t::activity, _( "hsh!" ) );
    }
    if( !who.knows_trap( p ) ) {
        get_map().maybe_trigger_trap( p, who, true );
    }
}

void construct::do_turn_exhume( const tripoint_bub_ms &p, Character &who )
{
    do_turn_shovel( p, who );
    if( !who.has_morale( morale_gravedigger ) ) {
        if( who.has_trait( trait_SPIRITUAL ) && !who.has_trait( trait_PSYCHOPATH ) )  {
            if( who.query_yn(
                    _( "Would you really touch the sacred resting place of the dead?" ) ) ) {
                add_msg( m_info, _( "Exhuming a grave is really against your beliefs." ) );
                who.add_morale( morale_gravedigger, -50, -100, 48_hours, 12_hours );
                if( one_in( 3 ) ) {
                    who.vomit();
                }
            } else {
                who.activity.set_to_null();
            }
        } else if( who.has_trait( trait_PSYCHOPATH ) ) {
            who.add_msg_if_player(
                m_good, _( "Exhuming a grave is fun now, when there is no one to object." ) );
            who.add_morale( morale_gravedigger, 25, 50, 2_hours, 1_hours );
        } else if( who.has_trait( trait_NUMB ) ) {
            who.add_msg_if_player( m_bad, _( "You wonder if you dig up anything useful." ) );
            who.add_morale( morale_gravedigger, -25, -50, 2_hours, 1_hours );
        } else if( !who.has_trait( trait_EATDEAD ) && !who.has_trait( trait_SAPROVORE ) ) {
            who.add_msg_if_player( m_bad, _( "Exhuming this grave is utterly disgusting!" ) );
            who.add_morale( morale_gravedigger, -25, -50, 2_hours, 1_hours );
            if( one_in( 5 ) ) {
                who.vomit();
            }
        }
    }
}

void construct::failure_standard( const tripoint_bub_ms & )
{
    add_msg( m_info, _( "You cannot build there!" ) );
}

void construct::failure_deconstruct( const tripoint_bub_ms & )
{
    add_msg( m_info, _( "You cannot deconstruct this!" ) );
}

template <typename T>
void assign_or_debugmsg( T &dest, const std::string &fun_id,
                         const std::map<std::string, T> &possible )
{
    const auto iter = possible.find( fun_id );
    if( iter != possible.end() ) {
        dest = iter->second;
    } else {
        dest = possible.find( "" )->second;
        const std::string list_available = enumerate_as_string( possible.begin(), possible.end(),
        []( const std::pair<std::string, T> &pr ) {
            return pr.first;
        } );
        debugmsg( "Unknown function: %s, available values are %s", fun_id.c_str(), list_available );
    }
}

void load_construction( const JsonObject &jo )
{
    construction con;
    // These ids are only temporary. The actual ids are determined in finalize_construction,
    // after removing blacklisted constructions.
    con.id = construction_id( -1 );
    con.str_id = construction_str_id( jo.get_string( "id" ) );
    if( con.str_id.is_null() ) {
        jo.throw_error_at( "id", "Null construction id specified" );
    } else if( construction_id_map.find( con.str_id ) != construction_id_map.end() ) {
        jo.throw_error_at( "id", "Duplicate construction id" );
    }

    jo.get_member( "group" ).read( con.group );
    if( jo.has_member( "required_skills" ) ) {
        for( JsonArray arr : jo.get_array( "required_skills" ) ) {
            con.required_skills[skill_id( arr.get_string( 0 ) )] = arr.get_int( 1 );
        }
    } else {
        skill_id legacy_skill( jo.get_string( "skill", skill_fabrication.str() ) );
        int legacy_diff = jo.get_int( "difficulty" );
        con.required_skills[ legacy_skill ] = legacy_diff;
    }

    con.category = construction_category_id( jo.get_string( "category", "OTHER" ) );
    if( jo.has_string( "time" ) ) {
        con.time = to_moves<int>( read_from_json_string<time_duration>( jo.get_member( "time" ),
                                  time_duration::units ) );
    }

    const requirement_id req_id( "inline_construction_" + con.str_id.str() );
    requirement_data::load_requirement( jo, req_id );
    con.requirements = req_id;

    if( jo.has_string( "using" ) ) {
        con.reqs_using = { { requirement_id( jo.get_string( "using" ) ), 1} };
    } else if( jo.has_array( "using" ) ) {
        for( JsonArray cur : jo.get_array( "using" ) ) {
            con.reqs_using.emplace_back( requirement_id( cur.get_string( 0 ) ), cur.get_int( 1 ) );
        }
    }

    jo.read( "pre_note", con.pre_note );
    con.pre_terrain = jo.get_as_string_set( "pre_terrain" );
    if( !con.pre_terrain.empty() ) {
        const std::string &first_pre_terrain = *con.pre_terrain.begin();
        if( first_pre_terrain.size() > 1
            && first_pre_terrain[0] == 'f'
            && first_pre_terrain[1] == '_' ) {
            con.pre_is_furniture = true;
        }
    }

    con.post_terrain = jo.get_string( "post_terrain", "" );
    if( con.post_terrain.size() > 1
        && con.post_terrain[0] == 'f'
        && con.post_terrain[1] == '_' ) {
        con.post_is_furniture = true;
    }

    std::string activity_level = jo.get_string( "activity_level", "MODERATE_EXERCISE" );
    const auto activity_it = activity_levels_map.find( activity_level );
    if( activity_it != activity_levels_map.end() ) {
        con.activity_level = activity_it->second;
    } else {
        jo.throw_error( string_format( "Invalid activity level %s in construction %s", activity_level,
                                       con.str_id.str() ) );
    }

    if( jo.has_member( "pre_flags" ) ) {
        con.pre_flags.clear();
        if( jo.has_string( "pre_flags" ) ) {
            con.pre_flags.emplace( jo.get_string( "pre_flags" ), false );
        } else if( jo.has_object( "pre_flags" ) ) {
            JsonObject jflag = jo.get_object( "pre_flags" );
            con.pre_flags.emplace( jflag.get_string( "flag" ), jflag.get_bool( "force_terrain" ) );
        } else if( jo.has_array( "pre_flags" ) ) {
            for( JsonValue jval : jo.get_array( "pre_flags" ) ) {
                if( jval.test_string() ) {
                    con.pre_flags.emplace( jval.get_string(), false );
                } else if( jval.test_object() ) {
                    JsonObject jflag = jval.get_object();
                    con.pre_flags.emplace( jflag.get_string( "flag" ), jflag.get_bool( "force_terrain" ) );
                }
            }
        }
    }

    con.post_flags = jo.get_tags( "post_flags" );

    if( jo.has_member( "byproducts" ) ) {
        con.byproduct_item_group = item_group::load_item_group( jo.get_member( "byproducts" ),
                                   "collection", "byproducts of construction " + con.str_id.str() );
    }

    static const std::map<std::string, bool( * )( const tripoint_bub_ms & )> pre_special_map = {{
            { "", construct::check_nothing },
            { "check_channel", construct::check_channel },
            { "check_empty_lite", construct::check_empty_lite },
            { "check_empty", construct::check_empty },
            { "check_unblocked", construct::check_unblocked },
            { "check_support", construct::check_support },
            { "check_support_below", construct::check_support_below },
            { "check_single_support", construct::check_single_support },
            { "check_stable", construct::check_stable },
            { "check_nofloor_above", construct::check_nofloor_above },
            { "check_deconstruct", construct::check_deconstruct },
            { "check_up_OK", construct::check_up_OK },
            { "check_down_OK", construct::check_down_OK },
            { "check_no_trap", construct::check_no_trap },
            { "check_ramp_high", construct::check_ramp_high },
            { "check_no_wiring", construct::check_no_wiring },
            { "check_matching_down_above", construct::check_matching_down_above }
        }
    };
    static const std::map<std::string, void( * )( const tripoint_bub_ms &, Character & )>
    post_special_map = {{
            { "", construct::done_nothing },
            { "done_trunk_plank", construct::done_trunk_plank },
            { "done_grave", construct::done_grave },
            { "done_vehicle", construct::done_vehicle },
            { "done_appliance", construct::done_appliance },
            { "done_wiring", construct::done_wiring },
            { "done_deconstruct", construct::done_deconstruct },
            { "done_dig_grave", construct::done_dig_grave },
            { "done_dig_grave_nospawn", construct::done_dig_grave_nospawn },
            { "done_dig_stair", construct::done_dig_stair },
            { "done_mine_downstair", construct::done_mine_downstair },
            { "done_mine_upstair", construct::done_mine_upstair },
            { "done_wood_stairs", construct::done_wood_stairs },
            { "done_window_curtains", construct::done_window_curtains },
            { "done_extract_maybe_revert_to_dirt", construct::done_extract_maybe_revert_to_dirt },
            { "done_mark_firewood", construct::done_mark_firewood },
            { "done_mark_practice_target", construct::done_mark_practice_target },
            { "done_ramp_low", construct::done_ramp_low },
            { "done_ramp_high", construct::done_ramp_high },
            { "add_matching_down_above", construct::add_matching_down_above },
            { "remove_above", construct::remove_above },
            { "add_roof", construct::add_roof }

        }
    };
    static const std::map<std::string, void( * )( const tripoint_bub_ms &, Character & )>
    do_turn_special_map = {{
            { "", construct::done_nothing },
            { "do_turn_deconstruct", construct::do_turn_deconstruct },
            { "do_turn_shovel", construct::do_turn_shovel },
            { "do_turn_exhume", construct::do_turn_exhume },
        }
    };
    static const std::map<std::string, void( * )( const tripoint_bub_ms & )>
    explain_fail_map = {{
            { "standard", construct::failure_standard },
            { "deconstruct", construct::failure_deconstruct },
        }
    };
    std::string failure_fallback = "standard";
    if( jo.has_array( "pre_special" ) ) {
        JsonArray jarr = jo.get_array( "pre_special" );
        for( std::string special : jarr ) {
            if( special == "check_deconstruct" ) {
                failure_fallback =  "deconstruct";
            }
            assign_or_debugmsg( con.pre_special, special, pre_special_map );
            con.pre_specials.push_back( con.pre_special );
        }
    } else {
        const std::string special = jo.get_string( "pre_special", "" );
        if( special == "check_deconstruct" ) {
            failure_fallback =  "deconstruct";
        }
        assign_or_debugmsg( con.pre_special, special, pre_special_map );
    }
    if( jo.has_array( "post_special" ) ) {
        JsonArray jarr = jo.get_array( "post_special" );
        for( std::string special : jarr ) {
            assign_or_debugmsg( con.post_special, special, post_special_map );
            con.post_specials.push_back( con.post_special );
        }
    } else {
        assign_or_debugmsg( con.post_special, jo.get_string( "post_special", "" ), post_special_map );
    }
    assign_or_debugmsg( con.do_turn_special, jo.get_string( "do_turn_special", "" ),
                        do_turn_special_map );
    assign_or_debugmsg( con.explain_failure, jo.get_string( "explain_failure", failure_fallback ),
                        explain_fail_map );
    con.vehicle_start = jo.get_bool( "vehicle_start", false );

    con.on_display = jo.get_bool( "on_display", true );
    con.dark_craftable = jo.get_bool( "dark_craftable", false );
    con.strict = jo.get_bool( "strict", false );

    constructions.push_back( con );
    construction_id_map.emplace( con.str_id, con.id );
}

void reset_constructions()
{
    constructions.clear();
    construction_id_map.clear();
    finalized = false;
}

void check_constructions()
{
    for( size_t i = 0; i < constructions.size(); i++ ) {
        const construction &c = constructions[ i ];
        const std::string display_name = "construction " + c.str_id.str();
        for( const auto &pr : c.required_skills ) {
            if( !pr.first.is_valid() ) {
                debugmsg( "Unknown skill %s in %s", pr.first.c_str(), display_name );
            }
        }

        if( !c.requirements.is_valid() ) {
            debugmsg( "%s has missing requirement data %s",
                      display_name, c.requirements.c_str() );
        }

        if( !c.pre_terrain.empty() ) {
            for( const auto &pre_terrain : c.pre_terrain ) {
                if( c.pre_is_furniture ) {
                    if( !furn_str_id( pre_terrain ).is_valid() ) {
                        debugmsg( "Unknown pre_terrain (furniture) %s in %s", pre_terrain, display_name );
                    }
                } else if( !ter_str_id( pre_terrain ).is_valid() ) {
                    debugmsg( "Unknown pre_terrain (terrain) %s in %s", pre_terrain, display_name );
                }
            }
        }
        if( !c.post_terrain.empty() ) {
            if( c.post_is_furniture ) {
                if( !furn_str_id( c.post_terrain ).is_valid() ) {
                    debugmsg( "Unknown post_terrain (furniture) %s in %s", c.post_terrain, display_name );
                }
            } else if( !ter_str_id( c.post_terrain ).is_valid() ) {
                debugmsg( "Unknown post_terrain (terrain) %s in %s", c.post_terrain, display_name );
            }
        }
        if( c.id != construction_id( i ) ) {
            debugmsg( "%s has id %u, but should have %u",
                      display_name, c.id.to_i(), i );
        }
        if( construction_id_map.find( c.str_id ) == construction_id_map.end() ) {
            debugmsg( "%s is an invalid string id",
                      display_name );
        } else if( construction_id_map[c.str_id] != construction_id( i ) ) {
            debugmsg( "%s points to int id %u, but should point to %u",
                      display_name, construction_id_map[c.str_id].to_i(), i );
        }
    }
}

int construction::print_time( const catacurses::window &w, const point &p, int width,
                              nc_color col ) const
{
    std::string text = get_time_string();
    return fold_and_print( w, p, width, col, text );
}

float construction::time_scale() const
{
    //incorporate construction time scaling
    if( get_option<int>( "CONSTRUCTION_SCALING" ) == 0 ) {
        return calendar::season_ratio();
    } else {
        return get_option<int>( "CONSTRUCTION_SCALING" ) / 100.0;
    }
}

int construction::adjusted_time() const
{
    int final_time = time;
    int assistants = 0;

    for( Character *elem : get_avatar().get_crafting_helpers() ) {
        if( elem->meets_skill_requirements( *this ) ) {
            assistants++;
        }
    }

    if( assistants >= 2 ) {
        final_time *= 0.4f;
    } else if( assistants == 1 ) {
        final_time *= 0.75f;
    }

    final_time *= time_scale();

    return final_time;
}

std::string construction::get_time_string() const
{
    const time_duration turns = time_duration::from_moves( adjusted_time() );
    return _( "Time to complete: " ) + colorize( to_string( turns ), color_data );
}

std::vector<std::string> construction::get_folded_time_string( int width ) const
{
    std::string time_text = get_time_string();
    std::vector<std::string> folded_time = foldstring( time_text, width );
    return folded_time;
}

void finalize_constructions()
{
    std::vector<item_comp> frame_items;
    for( const vpart_info &vpi : vehicles::parts::get_all() ) {
        if( !vpi.has_flag( flag_INITIAL_PART ) ) {
            continue;
        }
        if( vpi.id == vpart_frame ) {
            frame_items.insert( frame_items.begin(), { vpi.base_item, 1 } );
        } else {
            frame_items.emplace_back( vpi.base_item, 1 );
        }
    }

    if( frame_items.empty() ) {
        debugmsg( "No valid frames detected for vehicle construction" );
    }

    for( construction &con : constructions ) {
        if( !con.group.is_valid() ) {
            debugmsg( "Invalid construction group (%s) defined for construction (%s)",
                      con.group.str(), con.str_id.str() );
        }
        if( con.vehicle_start ) {
            const_cast<requirement_data &>( con.requirements.obj() ).get_components().push_back( frame_items );
        }
        bool is_valid_construction_category = false;
        for( const construction_category &cc : construction_categories::get_all() ) {
            if( con.category == cc.id ) {
                is_valid_construction_category = true;
                break;
            }
        }
        if( !is_valid_construction_category ) {
            debugmsg( "Invalid construction category (%s) defined for construction (%s)", con.category.str(),
                      con.str_id.str() );
        }
        requirement_data requirements_ = std::accumulate(
                                             con.reqs_using.begin(), con.reqs_using.end(), *con.requirements );

        requirement_data::save_requirement( requirements_, con.requirements );
        con.reqs_using.clear();
        inp_mngr.pump_events();
    }

    constructions.erase( std::remove_if( constructions.begin(), constructions.end(),
    [&]( const construction & c ) {
        return c.requirements->is_blacklisted();
    } ), constructions.end() );

    construction_id_map.clear();
    for( size_t i = 0; i < constructions.size(); i++ ) {
        constructions[ i ].id = construction_id( i );
        construction_id_map.emplace( constructions[i].str_id, constructions[i].id );
    }

    finalized = true;
}

build_reqs get_build_reqs_for_furn_ter_ids(
    const std::pair<std::map<ter_id, int>, std::map<furn_id, int>> &changed_ids )
{
    ter_id const &base_ter = ter_t_dirt.id();
    build_reqs total_reqs;

    if( !finalized ) {
        debugmsg( "get_build_reqs_for_furn_ter_ids called before finalization" );
        return total_reqs;
    }
    std::map<construction_id, int> total_builds;

    // iteratively recurse through the pre-terrains until the pre-terrain is empty, adding
    // the constructions to the total_builds map
    const auto add_builds = [&total_builds, &base_ter]( const construction & build, int count ) {
        if( total_builds.find( build.id ) == total_builds.end() ) {
            total_builds[build.id] = 0;
        }
        total_builds[build.id] += count;
        if( build.pre_terrain.size() > 1 ) {
            debugmsg( "get_build_reqs_for_furn_ter_ids tried to get reqs for %s which has multiple pre_terrain",
                      build.str_id.str() );
            return;
        }
        std::string build_pre_ter = build.pre_terrain.empty() ? "" : *build.pre_terrain.begin();
        while( !build_pre_ter.empty() ) {
            bool found_pre = false;
            // only consider DECORATE constructions if there's no other way to build the target
            // this will allow painting walls, but will skip un-painting walls as a way to make a wall
            for( bool allow_decorate : {
                     false, true
                 } ) {
                for( const construction &pre_build : constructions ) {
                    if( ( pre_build.category == construction_category_DECORATE ) != allow_decorate ) {
                        continue;
                    }
                    if( pre_build.category == construction_category_REPAIR ||
                        pre_build.category == construction_category_DECONSTRUCT ) {
                        continue;
                    }
                    std::string pre_build_pre_terrain = pre_build.pre_terrain.empty() ? "" : *
                                                        pre_build.pre_terrain.begin();
                    if( ( pre_build.post_terrain.empty() ||
                          ( !pre_build.post_is_furniture &&
                            ter_id( pre_build.post_terrain ) != base_ter ) ) &&
                        ( pre_build.pre_terrain.empty() ||
                          ( pre_build.post_is_furniture &&
                            ter_id( pre_build_pre_terrain ) == base_ter ) ) &&
                        pre_build.post_terrain == build_pre_ter &&
                        pre_build_pre_terrain != build.post_terrain ) {
                        if( pre_build.pre_terrain.size() > 1 ) {
                            debugmsg( "get_build_reqs_for_furn_ter_ids tried to recurse into %s which has multiple pre_terrain",
                                      pre_build.str_id.str() );
                            return;
                        }
                        if( total_builds.find( pre_build.id ) == total_builds.end() ) {
                            total_builds[pre_build.id] = 0;
                        }
                        total_builds[pre_build.id] += count;
                        build_pre_ter = pre_build_pre_terrain;
                        found_pre = true;
                        break;
                    }
                }
                if( found_pre ) {
                    break;
                }
            }
            if( !found_pre ) {
                break;
            }
        }
    };

    // go through the list of terrains and add their constructions and any pre-constructions
    // to the map of total builds
    for( const auto &ter_data : changed_ids.first ) {
        bool found = false;
        // only consider DECORATE constructions if there's no other way to build the target
        // this will allow painting walls, but will skip un-painting walls as a way to make a wall
        for( bool allow_decorate : {
                 false, true
             } ) {
            for( const construction &build : constructions ) {
                if( build.post_terrain.empty() || build.post_is_furniture ||
                    build.category == construction_category_REPAIR ||
                    build.category == construction_category_DECONSTRUCT ||
                    ( build.category == construction_category_DECORATE ) != allow_decorate ) {
                    continue;
                }
                if( ter_id( build.post_terrain ) == ter_data.first ) {
                    add_builds( build, ter_data.second );
                    found = true;
                    break;
                }
            }
            if( found ) {
                break;
            }
        }
    }
    // same, but for furniture
    for( const auto &furn_data : changed_ids.second ) {
        for( const construction &build : constructions ) {
            if( build.post_terrain.empty() || !build.post_is_furniture ||
                build.category == construction_category_REPAIR ) {
                continue;
            }
            if( furn_id( build.post_terrain ) == furn_data.first ) {
                add_builds( build, furn_data.second );
                break;
            }
        }
    }

    for( const auto &build_data : total_builds ) {
        const construction &build = build_data.first.obj();
        const int count = build_data.second;
        total_reqs.time += build.time * count;
        total_reqs.raw_reqs[build.requirements] += count;
        for( const auto &req_skill : build.required_skills ) {
            auto it = total_reqs.skills.find( req_skill.first );
            if( it == total_reqs.skills.end() || it->second < req_skill.second ) {
                total_reqs.skills[req_skill.first] = req_skill.second;
            }
        }
    }

    return total_reqs;
}

static const construction null_construction {};

template <>
const construction_str_id &construction_id::id() const
{
    if( !finalized ) {
        debugmsg( "construction_id::id called before finalization" );
        return construction_str_id::NULL_ID();
    } else if( is_valid() ) {
        return constructions[to_i()].str_id;
    } else {
        if( to_i() != -1 ) {
            debugmsg( "Invalid construction id %d", to_i() );
        }
        return construction_str_id::NULL_ID();
    }
}

template <>
const construction &construction_id::obj() const
{
    if( !finalized ) {
        debugmsg( "construction_id::obj called before finalization" );
        return null_construction;
    } else if( is_valid() ) {
        return constructions[to_i()];
    } else {
        debugmsg( "Invalid construction id %d", to_i() );
        return null_construction;
    }
}

template <>
bool construction_id::is_valid() const
{
    if( !finalized ) {
        debugmsg( "construction_id::is_valid called before finalization" );
        return false;
    }
    return to_i() >= 0 && static_cast<size_t>( to_i() ) < constructions.size();
}

template <>
construction_id construction_str_id::id() const
{
    if( !finalized ) {
        debugmsg( "construction_str_id::id called before finalization" );
        return construction_id( -1 );
    }
    auto it = construction_id_map.find( *this );
    if( it != construction_id_map.end() ) {
        return it->second;
    } else {
        if( !is_null() ) {
            debugmsg( "Invalid construction str id %s", str() );
        }
        return construction_id( -1 );
    }
}

template <>
const construction &construction_str_id::obj() const
{
    if( !finalized ) {
        debugmsg( "construction_str_id::obj called before finalization" );
        return null_construction;
    }
    auto it = construction_id_map.find( *this );
    if( it != construction_id_map.end() ) {
        return it->second.obj();
    } else {
        debugmsg( "Invalid construction str id %s", str() );
        return null_construction;
    }
}

template <>
bool construction_str_id::is_valid() const
{
    if( !finalized ) {
        debugmsg( "construction_str_id::is_valid called before finalization" );
        return false;
    }
    return construction_id_map.find( *this ) != construction_id_map.end();
}
