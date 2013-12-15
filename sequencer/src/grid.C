
/*******************************************************************************/
/* Copyright (C) 2007-2008 Jonathan Moore Liles                                */
/*                                                                             */
/* This program is free software; you can redistribute it and/or modify it     */
/* under the terms of the GNU General Public License as published by the       */
/* Free Software Foundation; either version 2 of the License, or (at your      */
/* option) any later version.                                                  */
/*                                                                             */
/* This program is distributed in the hope that it will be useful, but WITHOUT */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for   */
/* more details.                                                               */
/*                                                                             */
/* You should have received a copy of the GNU General Public License along     */
/* with This program; see the file COPYING.  If not,write to the Free Software */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */
/*******************************************************************************/

#include "grid.H"
#include "common.h"
#include "canvas.H"

#include "non.H"

#include "smf.H"

using namespace MIDI;

MIDI::event_list Grid::_clipboard;

Grid::Grid ( void )
{
    _name = NULL;
    _notes = NULL;
    _number = 0;
    _height = 0;

    _rd = new data;
    _rw = NULL;

    // we need to initialize it here.
    data *d = (data *)_rd;

    _mode = 0;
    _locked = 0;

    d->length = 0;

    _bpb = 4;
    /* how many grid positions there are per beat */
    _ppqn = 4;                        

    viewport.h = 32;
    viewport.w = 32;
    viewport.x = 0;
    viewport.y = 0;

    _playing = false;
    _start = _end = _index = 0;
}

Grid::~Grid ( void )
{
    DMESSAGE( "deleting grid" );

    if ( _name )
        free( _name );
    if ( _notes )
        free( _notes );

    if ( _rw )
        delete _rw;
    if ( _rd )
        delete _rd;

}

/* copy constructor */
Grid::Grid ( const Grid &rhs ) : sigc::trackable()
{
    _rd = new data( *rhs._rd );
    _rw = NULL;

    _name = rhs._name ? strdup( rhs._name ) : NULL;
    _notes = rhs._notes ? strdup( rhs._notes ) : NULL;
    _number = rhs._number;
    _height = rhs._height;

    _mode = 0;
    _locked = 0;
    _playing = false;
    _index = 0;
    _start = 0;
    _end = 0;

    _bpb = rhs._bpb;
    _ppqn = rhs._ppqn;

    viewport = rhs.viewport;
}

void
Grid::lock ( void )
{
    if ( ! _locked++ )
    {
        _rw = new data( *_rd );
        _change_count = 0;
    }
}

void
Grid::_unlock ( bool no_undo )
{
    if ( 0 == --_locked )
    {
        if ( !no_undo )
        {
            _history.push_back( const_cast<data *>( _rd ) );

            if ( _history.size() > MAX_UNDO + 1 )
            {
                data *d = _history.front();

                delete d;

                _history.pop_front();
            }
        }

        // swap the copy back in (atomically).
        _rd = (const data *)_rw;

        _rw = NULL;

        do_change_updates();
    }
}

void
Grid::unlock ( void )
{
    _unlock ( false );
}

void
Grid::unlock_no_undo ( void )
{
    _unlock ( true );
}

void
Grid::do_change_updates ( void )
{
    for (int i = 0; i < _change_count; i++)
        signal_events_change ( _change_updates[i].xt, _change_updates[i].y,
                               _change_updates[i].len, _change_updates[i].height );
}

// Queues an event change (height = 0: all)
void
Grid::change_update ( tick_t xt, int y, tick_t len, int height )
{
    if ( _change_count == 1 && _change_updates[0].height == 0 )
        return;         // Return if already change all

    // If max changes reached, convert to change all
    if ( _change_count == MAX_CHANGE_UPDATES || height == 0 )
    {
        _change_count = 1;
        _change_updates[0].xt = 0;
        _change_updates[0].y = 0;
        _change_updates[0].len = 0;
        return;
    }

    // See if updates can be merged (only bother to merge single rows)
    if ( _change_count == 1 && _change_updates[0].y == y
         && height == 1 && _change_updates[0].height == 1 )
    {
        ChangeUpdate *c = &_change_updates[0];

        if ( xt >= c->xt && xt <= c->xt + c->len )
        {
            tick_t end1, end2;

            end1 = xt + len;
            end2 = c->xt + c->len;
            c->len = max( end1, end2 ) - c->xt;
            return;
        }
        else if ( c->xt >= xt && c->xt <= xt + len )
        {
            tick_t end1, end2;

            end1 = xt + len;
            end2 = c->xt + c->len;
            c->len = max( end1, end2 ) - xt;
            c->xt = xt;
            return;
        }
    }

    _change_updates[_change_count].xt = xt;
    _change_updates[_change_count].y = y;
    _change_updates[_change_count].len = len;
    _change_count++;
}

void
Grid::change_update ( MIDI::event *e )
{
    change_update ( e->timestamp(), note_to_y ( e->note() ), e->note_duration(), 1 );
}

void
Grid::change_update_all ( void )
{
    change_update (0, 0, 0, 0);
}

event *
Grid::_event ( int x, int y, bool write ) const
{
    const data *d = const_cast< data * >(_rd);

    const event_list *r = write ? &_rw->events : &d->events;

    tick_t xt = x_to_ts(x);

    if ( r->empty() )
/* || xt  > _rd->length ) */
        return NULL;

    int note = y_to_note( y );

    for ( event *e = r->first(); e; e = e->next() )
    {
        if ( ! e->is_note_on() )
            continue;

        if ( e->note() != note )
            continue;

        tick_t ts = e->timestamp();
        tick_t l = 0;

        if ( e->linked() )
            l = e->link()->timestamp() - ts;
        else
            WARNING( "found unlinked event... event list is corrupt." );
        
        if ( xt >= ts && xt < ts + l )
            // this is a little nasty
            return const_cast<event *>(e);
    }

    return NULL;
}

void
Grid::clear ( void )
{
    lock();

    _rw->events.clear();
    change_update_all();

    unlock();
}

void
Grid::del ( int x, int y )
{
    lock();

    event *e = _event ( x, y, true );

    if ( e )
    {
        change_update ( e );

        if ( e->linked() )
            _rw->events.remove( e->link() );

        _rw->events.remove( e );
    }

    unlock();
}

int
Grid::next_note_x ( int x ) const
{
    for ( const event *e = _rd->events.first(); e; e = e->next() )
        if ( e->is_note_on() && (ts_to_x( e->timestamp() ) > (uint)x ) )
            return ts_to_x( e->timestamp() );

    return 0;
}

int
Grid::prev_note_x ( int x ) const
{
    for ( const event *e = _rd->events.last(); e; e = e->prev() )
        if ( e->is_note_on() && (ts_to_x( e->timestamp() ) < (uint)x) )
            return ts_to_x( e->timestamp() );

    return 0;
}


void
Grid::_fix_length ( void )
{
    tick_t beats = (unsigned long)(_rw->length / PPQN);
    tick_t rem = (unsigned long)_rw->length % PPQN;

    _rw->length = (rem ? (beats + 1) : beats) * PPQN;
}

/**  Trim the length of the grid to the last event */
void
Grid::trim ( void )
{
    lock();

    event *e = _rw->events.last();

    if ( e )
    {
        tick_t ts = e->timestamp();
        tick_t old_length = _rw->length;

        _rw->length = ts;

        _fix_length();

        if ( _rw->length != old_length )
        {
            if ( _rw->length > old_length )
                change_update( old_length, 0, _rw->length - old_length, 128 );
            else change_update( _rw->length, 0, old_length - _rw->length, 128 );
        }
    }

    unlock();
}

void
Grid::fit ( void )
{
    int hi, lo;

    _rd->events.hi_lo_note( &hi, &lo );

    viewport.h = abs( hi - lo ) + 1;

    viewport.y = note_to_y( hi );
}

/** Expand the length of the grid to the last event */
void
Grid::expand ( void )
{
    lock();

    event *e = _rw->events.last();

    if ( e )
    {
        tick_t ts = e->timestamp();
        tick_t old_length = _rw->length;

        if ( ts > _rw->length )
        {
            _rw->length = ts;
            _fix_length();
            change_update( old_length, 0, _rw->length - old_length, 128 );
        }
    }

    unlock();
}

/** returns true if there is a note event at x,y */
bool
Grid::is_set ( int x, int y ) const
{
    return _event( x, y, false );
}

void
Grid::put ( int x, int y, tick_t l, int velocity )
{

    int xl = ts_to_x( l );
    tick_t ts = x_to_ts( x );

    event *on = new event;
    event *off = new event;

    // Don't allow overlap (Why not?)
    if ( _event( x, y, false ) ||
         _event( x + xl, y, false ) )
        return;

    DMESSAGE( "put %d,%d", x, y );

    lock();

    int note = y_to_note( y );

    on->status( event::NOTE_ON );
    on->note( note );
    on->timestamp( ts );
    on->note_velocity( velocity );
    on->link( off );

    off->status( event::NOTE_OFF );
    off->note( note );
    off->timestamp( ts + l );
    off->note_velocity( velocity );
    off->link( on );

    _rw->events.insert( on );
    _rw->events.insert( off );

    change_update ( on );
    expand();

    unlock();
}


// void
// pattern::move ( int x, int y, int nx )
// {
//   event *e = _event( x, y, false );

//   if ( e )
//     e->timestamp( nx );
// }


void
Grid::move ( int x, int y, int nx, int ny )
{
    lock();

    event *e = _event( x, y, true );

    if ( e )
    {
        DMESSAGE( "moving note" );

        event *on = e,
            *off = e->link();

        change_update( on );

        _rw->events.unlink( on  );
        _rw->events.unlink( off );

        on->note( y_to_note( ny ) );

        tick_t l = on->note_duration();
        on->timestamp( x_to_ts( nx ) );
        on->note_duration( l );

        _rw->events.insert( off );
        _rw->events.insert( on );

        change_update( on );
    }

    unlock();
}


void
Grid::adj_velocity ( int x, int y, int n )
{
    lock();

    event *e = _event( x, y, true );

    if ( e )
    {
        DMESSAGE( "adjusting velocity" );

        {
            int v = e->note_velocity();
            int new_v = v + n;

            if ( new_v > 127 )
                new_v = 127;
            else if ( new_v <= 0 )
                new_v = 1;

            if ( new_v != v )
            {
                e->note_velocity( new_v );
                change_update ( e );
            }
        }
    }

    unlock();
}

void
Grid::adj_duration ( int x, int y, int l )
{
    lock();

    event *e = _event( x, y, true );

    if ( e )
    {
        DMESSAGE( "adjusting duration" );

        {
            tick_t len = e->note_duration();
            int v = ts_to_x( len ) + l;
            tick_t new_len = x_to_ts( v > 0 ? v : 1 );

            if ( new_len != len )
            {
                e->note_duration( new_len );
                _rw->events.sort( e->link() );
                change_update( e->timestamp(), note_to_y( e->note() ),
                               max( len, new_len ), 1 );
            }
        }
    }

    unlock();
}

void
Grid::set_duration ( int x, int y, int ex )
{
    if ( ex < 1 )
        return;

    lock();

    event *e = _event( x, y, true );

    if ( e )
    {
        DMESSAGE( "adjusting duration" );

        tick_t len = e->note_duration();
        tick_t new_len = x_to_ts( ex );

        if ( new_len != len )
        {
            e->note_duration( new_len );
            _rw->events.sort( e->link() );
            change_update( e->timestamp(), note_to_y( e->note() ),
                           max( len, new_len ), 1 );
        }
    }

    unlock();
}

void
Grid::get_note_properties ( int x, int y, note_properties *p ) const
{
    const event *e = _event( x, y, false );
    
    e->get_note_properties( p );

    p->start = p->start;
    p->duration = p->duration;
    p->note = note_to_y( p->note );
}

/* void */
/* Grid::set_note_properties ( int x, int y, const note_properties *p ) */
/* { */
/*     lock(); */

/*     const event *e = _event( x, y, true ); */
    
/*     e->set_note_properties( p ); */

/*     unlock(); */
/* } */




/** if there's a note at grid coordinates x,y, then adjust them to the beginning of the note */
int
Grid::get_start ( int *x, int *y ) const
{
    const event *e = _event( *x, *y, false );
    
    if ( e )
    {
        *x = ts_to_x( e->timestamp() );
        return 1;
    }
    else
        return 0;
}

void
Grid::set_end ( int x, int y, int ex )
{
    lock();

    event *e = _event( x, y, true );

    if ( e )
    {
        DMESSAGE( "adjusting duration" );

        tick_t len = e->note_duration();
        tick_t new_len = x_to_ts( ex ) - e->timestamp();

        if ( new_len > x_to_ts( 1 ) && new_len != len )
        {
            e->note_duration( new_len );
            _rw->events.sort( e->link() );
            change_update( e->timestamp(), note_to_y( e->note() ),
                           max( len, new_len ), 1 );
        }
    }

    unlock();
}

void
Grid::toggle_select ( int x, int y )
{
    lock();

    event *e = _event( x, y, true );

    if ( e )
    {
        if ( e->selected() )
            e->deselect();
        else
            e->select();

        change_update ( e );
    }

    unlock_no_undo();
}

/** copy selected notes to clipboard */
void
Grid::copy ( void )
{
    _rd->events.copy_selected( &_clipboard );
}

void
Grid::cut ( void )
{
    _rd->events.copy_selected( &_clipboard );

    lock();

    _rw->events.remove_selected();

    change_update_all();

    unlock();
}

void
Grid::selected_velocity ( int v )
{
    lock();

    _rw->events.selected_velocity( v );
    change_update_all();

    unlock_no_undo();
}

void
Grid::paste ( int offset )
{
    lock();
    
    _rw->events.paste( x_to_ts( offset ), &_clipboard );

    change_update_all();
    expand();

    unlock();

}

/** insert /l/ ticks of time after /x/ */
void
Grid::insert_time ( int l, int r )
{
    tick_t start = x_to_ts( l );
    tick_t end = x_to_ts( r );

    lock();

    _rw->events.insert_time( start, end - start );

    change_update_all();
    expand();

    unlock();
}

/** select all events in range (notes straddling the border will also be selected */
void
Grid::select ( int l, int r )
{
    tick_t start = x_to_ts( l );
    tick_t end = x_to_ts( r );

    lock();

    _rw->events.select( start, end );
    change_update_all();

    unlock_no_undo();
}

/** select all (note) events in rectangle */
void
Grid::select ( int l, int r, int t, int b )
{
    tick_t start = x_to_ts( l );
    tick_t end = x_to_ts( r );

    lock();

    _rw->events.select( start, end, y_to_note( t) , y_to_note( b ) );
    change_update_all();

    unlock_no_undo();
}

/** delete events from /x/ to /l/, compressing time. */
void
Grid::delete_time ( int l, int r )
{
    tick_t start = x_to_ts( l );
    tick_t end = x_to_ts( r );

    lock();

    _rw->events.delete_time( start, end );
    change_update_all();

    unlock();
}

void
Grid::select_none ( void )
{
    lock();

    _rw->events.select_none();
    change_update_all();

    unlock_no_undo();
}

void
Grid::select_all ( void )
{
    lock();

    _rw->events.select_all();
    change_update_all();

    unlock_no_undo();
}

void
Grid::invert_selection ( void )
{
    lock();

    _rw->events.invert_selection();
    change_update_all();

    unlock_no_undo();
}

void
Grid::delete_selected ( void )
{
    lock();

    _rw->events.remove_selected();
    change_update_all();

    unlock();
}

void
Grid::nudge_selected ( int l )
{
    long o = x_to_ts( abs( l ) );

    if ( l < 0 )
        o = 0 - o;

    lock();

//    MESSAGE( "moving by %ld", o );

    _rw->events.nudge_selected( o );
    change_update_all();

    unlock();
}

void
Grid::move_selected ( int l )
{
    tick_t o = x_to_ts( l );

    lock();

//    MESSAGE( "moving by %ld", o );

    _rw->events.move_selected( o );
    change_update_all();

    unlock();
}

void
Grid::crop ( int l, int r )
{
    lock();

    if ( (uint)r < ts_to_x( _rw->length ) )
        delete_time( r, ts_to_x( _rw->length ) );
    if ( l > 0 )
        delete_time( 0, l );

    change_update_all();
    trim();

    unlock();
}

void
Grid::crop ( int l, int r, int t, int b )
{
    lock();

    _rw->events.push_selection();

    select( l, r, t, b );

    _rw->events.invert_selection();
    _rw->events.remove_selected();

    _rw->events.pop_selection();

    crop( l, r );       // Does a change all update

    unlock();
}

int
Grid::min_selected ( void ) const
{
    return ts_to_x( _rd->events.selection_min() );
}

void
Grid::_relink ( void )
{
    _rw->events.relink();
}

/* Dump the event list -- used by pattern / phrase dumppers */
void
Grid::dump ( smf *f, int channel ) const
{
    data *d = const_cast<data *>(_rd);

    midievent me;

    for ( event *e = d->events.first(); e; e = e->next() )
    {
        me = *e;
        me.channel( channel );

        f->write_event( &me );
    }
}

void
Grid::print ( void ) const
{
    data *d = const_cast<data *>(_rd);

    for ( event *e = d->events.first(); e; e = e->next() )
        e->print();
}


/*  */

/** Invoke /draw_note/ function for every note in the viewport */
void
Grid::draw_notes ( draw_note_func_t draw_note, void *userdata ) const 
{
    data *d = const_cast< data *>( _rd );

    for ( const event *e = d->events.first(); e; e = e->next() )
    {
        if ( ! e->is_note_on() )
            continue;

        const tick_t ts = e->timestamp();

        ASSERT( e->link(), "found a non-linked note" );

        const tick_t tse = e->link()->timestamp();

        draw_note(
            ts,
            note_to_y( e->note() ),
            tse - ts,
            e->note_velocity(), 
            e->selected(),
            userdata );
    }
}

 /*******************************************/
 /* Generic accessors -- boy C++ is verbose */
 /*******************************************/

/** Returns the index (playhead) for this grid  */
tick_t
Grid::index ( void ) const
{
    /* FIXME: considering the type of tick_t, we really need some kind
     of locking here to insure that this thread doesn't read _index
     while the RT thread is writing it. */
    return _index;
}

bool
Grid::playing ( void ) const
{
    return _playing;
}

int
Grid::height ( void ) const
{
    return _height;
}

void
Grid::height ( int h )
{
    _height = h;
}

tick_t
Grid::length ( void ) const
{
    return _rd->length;
}

void
Grid::length ( tick_t l )
{
    lock();

    _rw->length = l;
    change_update_all();

    unlock();
}

void
Grid::bars ( int n )
{
    lock();
    
    _rw->length = n * _bpb * PPQN;
    _fix_length();
    change_update_all();

    unlock();

    // trim();
}

int
Grid::bars ( void ) const
{
    return beats() / _bpb;
}

int
Grid::beats ( void ) const
{
    return  _rd->length / PPQN;
}

int
Grid::division ( void ) const
{
    return _bpb * _ppqn;
}

int
Grid::subdivision ( void ) const
{
    return _ppqn;
}

int
Grid::ppqn ( void ) const
{
    return _ppqn;
}

/** set grid resolution to /n/, where /n/ is the denominator e.g. 1/n. */
void
Grid::resolution ( unsigned int n )
{
    float W = viewport.w / _ppqn;

    _ppqn = n;

    viewport.w = _ppqn * W;
 
    change_update_all();
    do_change_updates();
    signal_settings_change();
}

int
Grid::resolution ( void ) const
{
    return _ppqn;
}

int
Grid::number ( void ) const
{
    return _number;
}

void
Grid::name ( char *s )
{
    if ( _name ) free ( _name );

    _name = s;

    signal_settings_change();
}

const char *
Grid::name ( void ) const
{
    return _name;
}

void
Grid::notes ( char *s )
{
    if ( _notes ) free ( _notes );

    _notes = s;

    signal_settings_change();
}

char *
Grid::notes ( void ) const
{
    return _notes;
}

void
Grid::mode ( int m )
{
    _mode = m;

    /* can't do this in RT thread, sorry.  */
///    signal_settings_change();
}

int
Grid::mode ( void ) const
{
    return _mode;
}

void
Grid::undo ( void )
{
    if ( ! _history.size() )
        return;

    data *d = _history.back();
    
    _history.pop_back();

    // swap the copy back in (atomically).
    _rd = (const data *)d;
    
    _rw = NULL;

    change_update_all();
    do_change_updates();
}

/** return a pointer to a copy of grid's event list in raw form */
event_list *
Grid::events ( void ) const
{
    data * d = const_cast< data * >( _rd );

    return new event_list( d->events );
}

/** replace event list with a copy of /el/ */
void
Grid::events ( const event_list * el )
{
    lock();

    _rw->events = *el;

    change_update_all();
    unlock();
}
