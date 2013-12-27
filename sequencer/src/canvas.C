
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

/* This is a generic double-buffering, optimizing canvas interface to
   grids (patterns and phrases). It draws only what is necessary to keep
   the display up-to-date. Actual drawing functions are in draw.C */
#include <FL/Fl.H>
#include <FL/Fl_Cairo.H>

#include "canvas.H"
#include "pattern.H"
#include "common.h"

#include "non.H"
#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include "gui/ui.H"
#include <FL/Fl_Panzoomer.H>
#include <FL/Fl_Slider.H>

using namespace MIDI;
extern UI *ui;

extern Fl_Color velocity_colors[];
extern Fl_Color velocity_select_colors[];
const int ruler_height = 14;

const int DRAG_THRESHOLD = 4;   // Pixel threshold for certain drag operations to kick in (ALT drag velocity/duration changes)

#define IS_PATTERN (parent() == ui->pattern_tab)
#define IS_PHRASE (parent() == ui->phrase_tab)
#define IS_SEQUENCE (parent() == ui->sequence_tab)



class Canvas::Canvas_Panzoomer : public Fl_Panzoomer
{
    Fl_Offscreen backbuffer;

public:

    Canvas_Panzoomer( int X, int Y,int W, int H, const char *L = 0 )
        : Fl_Panzoomer(X,Y,W,H,L)
        {
            backbuffer = 0;
        }

    Canvas *canvas;

private:

    static void draw_dash ( tick_t x, int y, tick_t l, int color, int selected, void *userdata )
        {
            Canvas_Panzoomer *o = (Canvas_Panzoomer*)userdata;
            
            o->draw_dash( x,y,l,color,selected );
        }

    void draw_dash ( tick_t x, int y, tick_t w, int color, int selected ) const
        {
            if ( selected )
                color = FL_MAGENTA;
            else
                color = velocity_colors[ color ];
            
            Canvas *c = canvas;
    
            double VS = (double)this->h() / c->m.maxh;
            double HS = (double)this->w() / ( _xmax - _xmin );

            y = c->ntr( y );
            
            if ( y < 0 )
                return;
            
            y *= VS;
            
            fl_color( fl_color_average( color, FL_GRAY, 0.5 ) );

            fl_rectf(
                x * HS,
                y,
                (w * HS) + 0.5,
                1 * VS + 0.5 );
        }
    
protected:

    void draw_background ( int X, int Y, int W, int H )
        {

            /* DMESSAGE( "%s%s%s%s%s%s", */
            /*           damage() & FL_DAMAGE_CHILD ? "CHILD " : "", */
            /*           damage() & FL_DAMAGE_ALL ? "ALL " : "", */
            /*           damage() & FL_DAMAGE_USER1 ? "USER 1 ": "", */
            /*           damage() & FL_DAMAGE_EXPOSE ? "EXPOSE " : "", */
            /*           damage() & FL_DAMAGE_SCROLL ? "SCROLL " : "", */
            /*           damage() & FL_DAMAGE_OVERLAY ? "OVERLAY " : ""); */

            if ( ! backbuffer ||
                 ! ( damage() & FL_DAMAGE_USER1 ) )
            {
                if ( !backbuffer )
                    backbuffer = fl_create_offscreen( W, H );

                DMESSAGE( "redrawing preview" );

                fl_begin_offscreen(backbuffer);
         
                fl_rectf( 0, 0, W, H, color() );
                canvas->m.grid->draw_notes( draw_dash, this );

                fl_end_offscreen();
            }

            fl_copy_offscreen( X,Y,W,H,backbuffer,0, 0 );
        }
public:

    void resize ( int X, int Y, int W, int H )
        {
            Fl_Panzoomer::resize( X,Y,W,H );
            if ( backbuffer )
                fl_delete_offscreen( backbuffer );
            backbuffer = 0;
            redraw();
        }

    void draw_overlay ( void )
        {
            Canvas *c = canvas;

            double HS = (double)w() /(  _xmax - _xmin );

            tick_t new_x = c->grid()->x_to_ts( c->grid()->ts_to_x( c->grid()->index() ) );
            fl_color( fl_color_add_alpha( FL_RED, 100 ) );
            fl_line( x() + new_x * HS, y(), x() + new_x * HS, y() + h() );
        }

};

Canvas::Canvas ( int X, int Y, int W, int H, const char *L ) : Fl_Group( X,Y,W,H,L )
{
    _event_state = EVENT_STATE_NONE;

    { Fl_Box *o = new Fl_Box( X, Y, W, H - 75 );
        /* this is a dummy group where the canvas goes */
        Fl_Group::current()->resizable( o );
    }
    { Fl_Group *o = new Fl_Group( X, Y + H - 75, W, 75 );
        
        {
            Canvas_Panzoomer *o = new Canvas_Panzoomer( X, Y + H - 75, W - 14, 75 );
            o->canvas = this;
            o->box( FL_FLAT_BOX );
//        o->color(fl_color_average( FL_BLACK, FL_WHITE, 0.90 ));
            o->color( FL_BLACK );
//        o->color(FL_BACKGROUND_COLOR);
//        o->type( FL_HORIZONTAL );
            o->callback( cb_scroll, this );
            o->when( FL_WHEN_CHANGED );
            panzoomer = o;
        }
        
        {
            Fl_Slider *o = new Fl_Slider( X + W - 14, Y + H - panzoomer->h(), 14, panzoomer->h() );
            o->range( 1, 128 );
            o->step( 1 );
            o->type( FL_VERTICAL );
            o->tooltip( "Vertical Zoom" );
            o->callback( cb_scroll, this );
            vzoom = o;
        }
        o->end();
    }

    m.origin_x = m.origin_y = m.height = m.width = m.div_w = m.div_h = m.margin_top = m.margin_left = m.playhead = m.w = m.h = 0;
    _selection.x1 = _selection.x2 = _selection.y1 = _selection.y2 = 0;

    m.margin_top = ruler_height;

    m.draw = false;

//    m.current = m.previous = NULL;

    m.row_compact = true;

    m.maxh = 128;

    m.vp = NULL;

    m.grid = NULL;

    end();

    resize( X,Y,W,H);
}

Canvas::~Canvas ( ) 
{

}

void
Canvas::handle_event_change ( tick_t x, int y, tick_t length, int height )
{
    /* mark the song as dirty and pass the signal on */
    song.set_dirty();

    if (height == 0)
    {
      Grid *g = grid();
      panzoomer->x_value( g->x_to_ts( m.vp->x), g->x_to_ts( m.vp->w ), 0, g->length());
      redraw();
    }
    else damage_grid (x, y, length, height);
}

/** change grid to /g/, returns TRUE if new grid size differs from old */
void
Canvas::grid ( Grid *g )
{
    m.grid = g;

    if ( ! g )
        return;

    m.vp = &g->viewport;

    char *s = m.vp->dump();
    DMESSAGE( "viewport: %s", s );
    free( s );

    resize_grid();

    vzoom->range( 1, m.maxh );
    vzoom->value( m.vp->h );
    
    update_mapping();

    /* connect signals */
    /* FIXME: what happens when we do this twice? */
    g->signal_events_change.connect( mem_fun( this, &Canvas::handle_event_change ) );
    g->signal_settings_change.connect( signal_settings_change.make_slot() );
    
    redraw();

//     parent()->redraw();
    signal_settings_change();
}

/** keep row compaction tables up-to-date */
void
Canvas::_update_row_mapping ( void )
{
    /* reset */
    for ( int i = 128; i-- ; )
        m.rtn[i] = m.ntr[i] = -1;

    DMESSAGE( "updating row mapping" );

    /* rebuild */
    int r = 0;
    for ( int n = 0; n < 128; ++n )
    {
        if ( m.grid->row_name( n ) )
        {
            m.rtn[r] = n;
            m.ntr[n] = r;
            ++r;
        }
    }

    if ( m.row_compact && r )
        m.maxh = r;
    else
        m.maxh = 128;

    m.vp->h = min( m.vp->h, m.maxh );

    resize_grid();
}

/** update everything about mapping, leaving the viewport alone */
void
Canvas::update_mapping ( void )
{
    _update_row_mapping();

    adj_size();

//    int old_margin = m.margin_left;

    m.margin_left = 0;

    m.draw = false;

    m.grid->draw_row_names( this );

    m.draw = true;

/*     if ( m.margin_left != old_margin ) */
/*     { */
/* //        signal_resize(); */
/*         redraw(); */
/*     } */
/*     else */

    damage(FL_DAMAGE_USER1);

}

/** change grid mapping */
void
Canvas::changed_mapping ( void )
{
    update_mapping();

    m.vp->h = min( m.vp->h, m.maxh );

    if ( m.vp->y + m.vp->h > m.maxh )
        m.vp->y = (m.maxh / 2) - (m.vp->h / 2);
}

Grid *
Canvas::grid ( void )
{
    return m.grid;
}


/** recalculate node sizes based on physical dimensions */
void
Canvas::adj_size ( void )
{
    if ( ! m.vp )
        return;

    m.div_w = (m.width - m.margin_left) / m.vp->w;
    m.div_h = (m.height - m.margin_top) / m.vp->h;
}

/** reallocate buffers to match grid dimensions */
void
Canvas::resize_grid ( void )
{
    adj_size();
    
    DMESSAGE( "resizing grid %dx%d", m.vp->w, m.vp->h );

    Grid *g = grid();
    panzoomer->x_value( g->x_to_ts( m.vp->x), g->x_to_ts( m.vp->w ), 0, g->length());
    panzoomer->y_value( m.vp->y, m.vp->h, 0, m.maxh  );

    panzoomer->zoom_range( 2, 16 );

//    m.vp->w = max( 32, min( (int)(m.vp->w * n), 256 ) );

}

/** inform the canvas with new phsyical dimensions */
void
Canvas::resize ( int x, int y, int w, int h )
{
    m.origin_x = x;
    m.origin_y = y;

    m.width = w;
    m.height = h - 75;
    
    Fl_Group::resize(x,y,w,h);

    adj_size();
}



/***********/
/* Drawing */
/***********/

/** is /x/ within the viewport? */
bool
Canvas::viewable_x ( int x )
{
    return x >= m.vp->x && x < m.vp->x + m.vp->w;
}

static
int
gui_draw_ruler ( int x, int y, int w, int div_w, int div, int ofs )
{
    /* Across the top */

   
    fl_font( FL_TIMES, ruler_height );

    int h = ruler_height;

    fl_color( FL_BACKGROUND_COLOR );

    w += 100;            /* FIXME: hack */

    //  fl_rectf( x, y, x + (div_w * w), y + h );
    fl_rectf( x, y, (div_w * w), h );


    fl_color( FL_FOREGROUND_COLOR );

    fl_line( x + div_w / 2, y, x + div_w * w, y );

    char pat[40];
    int z = div;
    int i;
    for ( i = 0; i < w; i++ )
    {
        int k = ofs + i;
        if ( 0 == k % z )
        {
            int nx = x + (i * div_w) + (div_w / 2);

            fl_color( FL_FOREGROUND_COLOR );

            fl_line( nx, y, nx, y + h - 1 );

            sprintf( pat, "%i", 1 + (k / z ));

            fl_color( FL_FOREGROUND_COLOR );
            fl_draw( pat, nx + div_w / 2, y + h + 1 / 2 );
        }
    }

    return h;
}

static
int
gui_draw_string ( int x, int y, int w, int h, int color, const char *s, bool draw )
{
    int rw;

    if ( ! s )
        return 0;

    fl_font( FL_COURIER, min( h, 18 ) );

    rw = fl_width( s );

    if ( fl_not_clipped( x, y, rw, h ) && draw )
    {
//        fl_rectf( x,y,w,h, FL_BACKGROUND_COLOR );

        if ( color )
            fl_color( velocity_colors[ color ] );
        else
            fl_color( FL_DARK_CYAN );

        fl_draw( s, x, y + h / 2 + fl_descent() );
    }

    return rw;
}

/** callback called by Grid::draw_row_names() to draw an individual row name  */
void
Canvas::draw_row_name ( int y, const char *name, int color )
{
    bool draw = m.draw;

    y = ntr( y );

    y -= m.vp->y;

    int bx = m.origin_x;
    int by = m.origin_y + m.margin_top + y * m.div_h;
    int bw = m.margin_left;
    int bh = m.div_h;

    if ( y < 0 || y >= m.vp->h )
        draw = false;

    if ( draw && name )
    {
        fl_rectf( bx, by, bw, bh, index(name, '#') ? FL_GRAY : FL_BLACK );
        fl_rect( bx, by, bw, bh, FL_BLACK );
    }
    
    m.margin_left = max( m.margin_left, gui_draw_string( bx + 1, by + 2,
                                                         bw - 1, bh - 4,
                                                         color,
                                                         name,
                                                         draw ) );    
}

void
Canvas::draw_mapping ( void )
{
//    int old_margin = m.margin_left;

    m.margin_left = 0;

    m.draw = false;

    m.grid->draw_row_names( this );

    adj_size();

    m.draw = true;

    m.grid->draw_row_names( this );
}

void
Canvas::draw_ruler ( void )
{
    m.margin_top = gui_draw_ruler( m.origin_x + m.margin_left, 
                                   m.origin_y, 
                                   m.vp->w,
                                   m.div_w,
                                   m.grid->division(),
                                   m.vp->x);
}

void
Canvas::damage_grid ( tick_t x, int y, tick_t w, int h = 1 )
{
    y = ntr( y );

    if ( y < 0 )
        return;
   
    // adjust for viewport.
   
    x = m.grid->ts_to_x(x);
    w = m.grid->ts_to_x(w);
   
    x -= m.vp->x;
    y -= m.vp->y;

    if ( x < 0 || y < 0 || x >= m.vp->w || y >= m.vp->h )
        return;
   
    damage(FL_DAMAGE_USER1, m.origin_x + m.margin_left + x * m.div_w,
           m.origin_y + m.margin_top + y * m.div_h,
           m.div_w * w,
           m.div_h * h );
}

void
Canvas::draw_dash ( tick_t x, int y, tick_t w, int color, int selected ) const
{
    if ( !m.grid->velocity_sensitive() )
        color = 127;

    color = selected ? velocity_select_colors[ color ] : velocity_colors[ color ];

    y = ntr( y );
    
    if ( y < 0 )
        return;

    // adjust for viewport.

    x = m.grid->ts_to_x(x);
    w = m.grid->ts_to_x(w);

    x -= m.vp->x;
    y -= m.vp->y;

    x = m.origin_x + m.margin_left + x * m.div_w;
    y = m.origin_y + m.margin_top + y * m.div_h;
    w *= m.div_w;

    if ( w > 4 ) fl_rectf( x + 1, y + 1, w - 1, m.div_h - 1, color );
}

/** callback used by Grid::draw()  */
void
Canvas::draw_dash ( tick_t x, int y, tick_t w, int color, int selected, void *userdata )
{
    Canvas *o = (Canvas*)userdata;
    
    o->draw_dash( x,y,w,color,selected );
}

int
Canvas::playhead_moved ( void )
{
    int x = m.grid->ts_to_x( m.grid->index() );
    
    return m.playhead != x;
}

void
Canvas::redraw_playhead ( void )
{
    int old_x = m.playhead;
    
    int new_x = m.grid->ts_to_x( m.grid->index() );

    if ( old_x != new_x )
    {
        window()->damage( FL_DAMAGE_OVERLAY );
    }

    if ( m.playhead < m.vp->x || m.playhead >= m.vp->x + m.vp->w )
    {
        if ( config.follow_playhead )
        {
            new_x = m.playhead;

            panzoomer->x_value( m.grid->index() );
            panzoomer->do_callback();
        }
    }
}

void
Canvas::draw_overlay ( void )
{
    if ( ! visible_r() )
        return;

    fl_push_clip( x() + m.margin_left,
                  y() + m.margin_top,
                  w() - m.margin_left,
                  h() - panzoomer->h() - m.margin_top );

    draw_playhead();

    if ( _selection.x1 != 0 || _selection.x2 != 0 )
    {
        int X1,Y1,X2,Y2,W,H;

        SelectionRect &s = _selection;

        X1 = min( s.x1, s.x2 ) - m.vp->x;
        X2 = max( s.x1, s.x2 ) - m.vp->x;

        if ( s.y1 != 0 || s.y2 != 128 )
        {
            Y1 = ntr ( min( s.y1, s.y2 ) ) - m.vp->y;
            Y2 = ntr ( max( s.y1, s.y2 ) ) - m.vp->y;
            H = (Y2 - Y1) * m.div_h;
            Y1 = Y1 * m.div_h + m.origin_y + m.margin_top;
        }
        else
        {
            Y1 = m.origin_y + m.margin_top - 1;
            H = h() - panzoomer->h() - m.margin_top + 2;
        }

        W = (X2 - X1) * m.div_w;
        X1 = X1 * m.div_w + m.origin_x + m.margin_left;

        bool select_state = _event_state == EVENT_STATE_SELECT_RECTANGLE
            || _event_state == EVENT_STATE_SELECT_RANGE;

        fl_rect( X1, Y1, W, H, select_state ? FL_BLUE : fl_color_add_alpha ( FL_BLUE, 100 ) );
    }

    fl_pop_clip();

    panzoomer->draw_overlay();
}

/** draw only the playhead--without reexamining the grid */
void
Canvas::draw_playhead ( void )
{
    int x = m.grid->ts_to_x( m.grid->index() );

    /* if ( m.playhead == x ) */
    /*     return; */

    m.playhead = x;

    /* if ( m.playhead < m.vp->x || m.playhead >= m.vp->x + m.vp->w ) */
    /*     return; */

    int px = m.origin_x + m.margin_left + ( x - m.vp->x ) * m.div_w;

    int X,Y,W,H;
 
    X = px;
    Y = m.origin_y + m.margin_top;
    W = m.div_w;
    H = m.origin_y + m.margin_top + m.vp->h * m.div_h;

    cairo_set_operator( Fl::cairo_cc(), CAIRO_OPERATOR_HSL_COLOR );

    fl_rectf( X,Y,W,H, FL_RED );

    cairo_set_operator( Fl::cairo_cc(), CAIRO_OPERATOR_OVER );

    fl_rect( X,Y,W,H, FL_RED );
}

void
Canvas::draw_clip ( void *v, int X, int Y, int W, int H )
{
    fl_push_clip (X, Y, W, H);
    ((Canvas*)v)->draw_clip();
    fl_pop_clip ();
}

void
Canvas::draw_clip ( void )
{
    int X, Y, W, H;

    box( FL_FLAT_BOX );
    labeltype( FL_NO_LABEL );

    X = m.origin_x + m.margin_left;
    Y = m.origin_y + m.margin_top;
    W = w() - m.margin_left;
    H = h() - m.margin_top - panzoomer->h();

    fl_push_clip( X, Y, W, H );

    fl_rectf( X, Y, W, H, FL_BLACK );

    /* draw bar/beat lines */

    for ( int gx = m.vp->x;
          gx < m.vp->x + m.vp->w;
          gx++ )
    {
        if ( m.grid->x_to_ts( gx ) > m.grid->length() )
            break;

        if ( gx % m.grid->division() == 0 )
            fl_color( fl_color_average( FL_GRAY, FL_BLACK, 0.80 ) );
        else if ( gx % m.grid->subdivision() == 0 )
            fl_color( fl_color_average( FL_GRAY, FL_BLACK, 0.40 ) );
        else
            continue;
        
        fl_rectf( X + ( ( gx - m.vp->x ) * m.div_w ),
                  Y, 
                  m.div_w, 
                  H );
    }

    m.grid->draw_notes( draw_dash, this );

    fl_color( fl_color_add_alpha( fl_rgb_color( 127,127,127 ), 50 ));

    /* draw grid */
    
    fl_begin_line();
    
    if ( m.div_w > 4 )
    {
        for ( int gx = X; gx < X + W; gx += m.div_w )
        {
            fl_vertex( gx, Y );
            fl_vertex( gx, Y + H );
            fl_gap();
        }
    }

    if ( m.div_h > 2 )
    {
        for ( int gy = Y; gy < Y + H; gy += m.div_h )
        {
            fl_vertex( X, gy );
            fl_vertex( X + W, gy );
            fl_gap();
        }
    }

    fl_end_line();

    fl_pop_clip();
}


/** draw ONLY those nodes necessary to bring the canvas up-to-date with the grid */
void
Canvas::draw ( void )
{
    box( FL_NO_BOX );
    labeltype( FL_NO_LABEL );

    /* DMESSAGE( "%s%s%s%s%s%s", */
    /*           damage() & FL_DAMAGE_CHILD ? "CHILD " : "", */
    /*           damage() & FL_DAMAGE_ALL ? "ALL " : "", */
    /*           damage() & FL_DAMAGE_USER1 ? "USER 1 ": "", */
    /*           damage() & FL_DAMAGE_EXPOSE ? "EXPOSE " : "", */
    /*           damage() & FL_DAMAGE_SCROLL ? "SCROLL " : "", */
    /*           damage() & FL_DAMAGE_OVERLAY ? "OVERLAY " : ""); */


    if ( damage() & FL_DAMAGE_SCROLL )
    {
        int dx = ( _old_scroll_x - m.vp->x ) * m.div_w;
        int dy = ( _old_scroll_y - m.vp->y ) * m.div_h;

        fl_scroll( m.origin_x + m.margin_left,
                   m.origin_y + m.margin_top,
                   w() - m.margin_left,
                   h() - m.margin_top - panzoomer->h(),
                   dx, dy, 
                   draw_clip,
                   this );

        if ( dx )
            draw_ruler();

        if ( dy )
            draw_mapping();

        if ( damage() & FL_DAMAGE_CHILD )
            clear_damage( FL_DAMAGE_CHILD );
    }
    else if ( damage() & ~FL_DAMAGE_CHILD )
    {
        if (!( damage() & FL_DAMAGE_USER1))
        {
          draw_mapping();
          draw_ruler();
        }

        draw_clip ();
    }

    draw_children();

    _old_scroll_x = m.vp->x;
    _old_scroll_y = m.vp->y;
}

void
Canvas::cb_scroll ( Fl_Widget *w, void *v )
{
    ((Canvas*)v)->cb_scroll( w );
}

void
Canvas::cb_scroll ( Fl_Widget *w )
{
    if ( w == panzoomer )
    {
        Fl_Panzoomer *o = (Fl_Panzoomer*)w;

        if ( o->zoom_changed() )
        {
            m.vp->w = m.grid->division() * o->zoom();
            resize_grid();
            redraw();
        }
        else
        {
            int X, Y;

            X = grid()->ts_to_x( o->x_value() );
            Y = o->y_value();

            if ( m.vp->x != X || m.vp->y != Y )
            {
                m.vp->x = X;
                m.vp->y = Y;
                damage( FL_DAMAGE_SCROLL );
            }
        }
    }
    else if ( w == vzoom )
    {
        Fl_Slider *o = (Fl_Slider*)w;
        
        float n = o->value();

        m.vp->h = min( (int)n, m.maxh );

        resize_grid();
        
        song.set_dirty();

        redraw();
    }

}


/** convert pixel coords into grid coords. returns true if valid */
bool
Canvas::grid_pos ( int *x, int *y ) const
{
    /* if ( ( *x < m.origin_x + m.margin_left ) || */
    /*        ( *y < m.origin_y + m.margin_top ) || */
    /*          ( *x > m.origin_x + w() ) || */
    /*        (*y > m.origin_y + h() - panzoomer->h() ) ) */
    /*     return false; */

    *y = (*y - m.margin_top - m.origin_y) / m.div_h;
    *x = (*x - m.margin_left - m.origin_x) / m.div_w;

    /* if ( *x < 0 || *y < 0 || *x >= m.vp->w || *y >= m.vp->h ) */
    /*     return false; */

    /* adjust for viewport */
    *x += m.vp->x;
    *y += m.vp->y;

    /* adjust for row-compaction */
    *y = rtn( *y );

    return true;
}



/******************/
/* Input handlers */
/******************/

/* These methods translate viewport pixel coords to absolute grid
   coords and pass on to the grid. */

/** if coords correspond to a row name entry, return the (absolute) note number, otherwise return -1 */
int
Canvas::is_row_press ( void ) const
{
    if ( Fl::event_inside( this->x(),
                           this->y() + this->m.margin_top,
                           this->m.margin_left,
                           ( this->h() - this->m.margin_top ) - this->panzoomer->h() ) )
    {
        int dx,dy;
        dx = Fl::event_x();
        dy = Fl::event_y();

        grid_pos( &dx, &dy );

        return m.grid->y_to_note(dy );
    }
    else
        return -1;
}

bool
Canvas::is_ruler_click ( void ) const
{
    return Fl::event_y() < m.origin_y + m.margin_top;
}

void
Canvas::adj_length ( int x, int y, int n )
{
    if ( ! grid_pos( &x, &y ) )
        return;

    m.grid->adj_duration( x, y, n );
}

void
Canvas::set_end ( int x, int y, int n )
{
    if ( ! grid_pos( &x, &y ) )
        return;

    m.grid->set_end( x, y, n );
}

void
Canvas::select ( int x, int y )
{
    if ( ! grid_pos( &x, &y ) )
        return;

    m.grid->toggle_select( x, y );
}

void
Canvas::move_selected ( int dir, int n )
{
    switch ( dir )
    {
        case RIGHT:
            m.grid->set_undo_group ("move");
            m.grid->nudge_selected( n );
            break;
        case LEFT:
            m.grid->set_undo_group ("move");
            m.grid->nudge_selected( 0 - n );            // handles clamping
            break;
        case UP:
        case DOWN:
        {
            event_list *el = m.grid->events();
            int row_min = 127, row_max = 0, row;

            // Find range of rows of selected notes for current row compaction
            for ( event *e = el->first() ; e ; e = e->next() )
            {
                if ( ! e->is_note_on() ) continue;

                row = ntr ( m.grid->note_to_y ( e->note() ) );        // Convert to row
                if ( row == -1 ) continue;

                if ( row > row_max ) row_max = row;
                if ( row < row_min ) row_min = row;
            }

            if ( row_min == 127 && row_max == 0 )
                return;         // No matching events?

            if ( dir == UP )
            {
                if ( n > row_min ) n = row_min;
                if ( n == 0 ) return;

                for ( int y = row_min; y <= row_max; ++y )
                    el->rewrite_selected( m.grid->y_to_note( rtn( y ) ), m.grid->y_to_note( rtn( y - n ) ) );
            }
            else
            {
                if ( n >= m.maxh - row_max ) n = m.maxh - row_max - 1;
                if ( n == 0 ) return;

                for ( int y = row_max; y >= row_min; --y )
                    el->rewrite_selected( m.grid->y_to_note( rtn( y ) ), m.grid->y_to_note( rtn( y + n ) ) );
            }

            m.grid->set_undo_group ("move");
            m.grid->events( el );

            delete el;
            break;
        }
    }
}

void
Canvas::select_range ( void )
{
    if ( _selection.y1 == 0 && _selection.y2 == 128 )
        m.grid->select( _selection.x1, _selection.x2 );
    else
        m.grid->select( _selection.x1, _selection.x2, _selection.y1, _selection.y2 );
}

void
Canvas::invert_selection ( void )
{
    m.grid->invert_selection();
}

void
Canvas::crop ( void )
{
    if ( _selection.y1 == 0 && _selection.y2 == 128 )
        m.grid->crop( _selection.x1, _selection.x2 );
    else
        m.grid->crop( _selection.x1, _selection.x2, _selection.y1, _selection.y2 );

    m.vp->x = 0;

    _selection.x2 = _selection.x2 - _selection.x1;
   _selection.x1 = 0;
}

void
Canvas::delete_time ( void )
{
    m.grid->delete_time( _selection.x1, _selection.x2 );
}


void
Canvas::insert_time ( void )
{
    m.grid->insert_time( _selection.x1, _selection.x2 );
}

/** paste range as new grid */
void
Canvas::duplicate_range ( void )
{
    Grid *g = m.grid->clone();

    g->crop( _selection.x1, _selection.x2 );
    g->viewport.x = 0;
    grid( g );          // Select new grid
    ui->update_sequence_widgets();              // number of phrases may have changed.
}

void
Canvas::cut ( void )
{
    m.grid->cut();
}

void
Canvas::copy ( void )
{
    m.grid->copy();
}

void
Canvas::paste ( void ) 
{
    if ( _selection.x1 != _selection.x2 && _selection.x1 > m.vp->x && _selection.x1 < m.vp->x + m.vp->w )
        m.grid->paste( _selection.x1 );
    else
        m.grid->paste( m.vp->x );
}

void
Canvas::row_compact ( int n )
{
    switch ( n )
    {
        case OFF:
            m.row_compact = false;
            m.maxh = 128;
            break;
        case ON:
            m.row_compact = true;
            m.vp->y = 0;
            _update_row_mapping();
            break;
        case TOGGLE:
            row_compact( m.row_compact ? OFF : ON );
            break;
    }
//    _reset();
}

void
Canvas::pan ( int dir, int n )
{

    switch ( dir )
    {
        case LEFT: case RIGHT: case TO_PLAYHEAD: case TO_NEXT_NOTE: case TO_PREV_NOTE:
            /* handle horizontal movement specially */
            n *= m.grid->division();
            break;
        default:
            n *= 5;
            break;
    }

    switch ( dir )
    {
        case LEFT:
            m.vp->x = max( m.vp->x - n, 0 );
            break;
        case RIGHT:
            m.vp->x += n;
            break;
        case TO_PLAYHEAD:
            m.vp->x = m.playhead - (m.playhead % m.grid->division());
            break;
        case UP:
            m.vp->y = max( m.vp->y - n, 0 );
            break;
        case DOWN:
            m.vp->y = min( m.vp->y + n, m.maxh - m.vp->h );
            break;
        case TO_NEXT_NOTE:
        {
            int x = m.grid->next_note_x( m.vp->x );
            m.vp->x = x - (x % m.grid->division() );
            break;
        }
        case TO_PREV_NOTE:
        {
            int x = m.grid->prev_note_x( m.vp->x );
            m.vp->x = x - (x % m.grid->division() );
            break;
        }
    }

    panzoomer->x_value( m.grid->x_to_ts( m.vp->x), m.grid->x_to_ts( m.vp->w ),
                        0, m.grid->length());
    panzoomer->y_value( m.vp->y, m.vp->h, 0, m.maxh  );
    redraw();
}

void
Canvas::can_scroll ( int *left, int *right, int *up, int *down )
{
    *left = m.vp->x;
    *right = -1;
    *up = m.vp->y;
    *down = m.maxh - ( m.vp->y + m.vp->h );
}


/** adjust horizontal zoom (* n) */
void
Canvas::h_zoom ( float n )
{
    m.vp->w = max( 32, min( (int)(m.vp->w * n), 256 ) );

    resize_grid();

    song.set_dirty();
}

void
Canvas::selected_velocity ( int v )
{
    grid()->selected_velocity( v );
}

void
Canvas::v_zoom_fit ( void )
{
    if ( ! m.grid )
        return;

    changed_mapping();

    m.vp->h = m.maxh;
    m.vp->y = 0;

    resize_grid();

    song.set_dirty();

}

/** adjust vertical zoom (* n) */
void
Canvas::v_zoom ( float n )
{
    m.vp->h = max( 1, min( (int)(m.vp->h * n), m.maxh ) );

    resize_grid();

    song.set_dirty();
}

void
Canvas::notes ( char *s )
{
    m.grid->notes( s );
}

char *
Canvas::notes ( void )
{
    return m.grid->notes();
}

/** Set selection region */
void
Canvas::set_selection( int x1, int x2, int y1, int y2 )
{
    _selection.x1 = x1;
    _selection.x2 = x2;
    _selection.y1 = y1;
    _selection.y2 = y2;
}

/** Fix selection to where x1 <= x2 and y1 <= y2 */
void
Canvas::fix_selection( void )
{
    int tmp;

    if ( _selection.x1 > _selection.x2 )
    {
        tmp = _selection.x1;
        _selection.x1 = _selection.x2;
        _selection.x2 = tmp;
    }

    if ( _selection.y1 > _selection.y2 )
    {
        tmp = _selection.y1;
        _selection.y1 = _selection.y2;
        _selection.y2 = tmp;
    }
}

int
Canvas::handle ( int ev )
{
    static int last_move_x = 0;
    static int last_move_y = 0;
    static int drag_x;
    static int drag_y;
    static int drag_velocity;           // Drag start note velocity
    static bool hdrag_threshold;        // Set to TRUE if horizontal drag threshold reached
    static bool vdrag_threshold;        // Set to TRUE if vertical drag threshold reached
    static note_properties drag_note;

    int x, y;
    x = Fl::event_x();
    y = Fl::event_y();

    switch ( ev )
    {
        case FL_FOCUS:
        case FL_UNFOCUS:
            return 1;
        case FL_ENTER:
        case FL_LEAVE:
            fl_cursor( FL_CURSOR_DEFAULT );
            return 1;
        case FL_MOVE:
        {
            if ( Fl::event_inside( this->x() + this->m.margin_left,
                                   this->y() + this->m.margin_top,
                                   this->w() - this->m.margin_left,
                                   ( this->h() - this->m.margin_top ) - this->panzoomer->h() ) )
                fl_cursor( FL_CURSOR_HAND );
            else
                fl_cursor( FL_CURSOR_DEFAULT );

            return 1;
        }
        case FL_KEYBOARD:
        {
            if ( _event_state != EVENT_STATE_NONE )     // Ignore key presses when in an event state to simplify things
                return 1;

            if ( Fl::event_state() & FL_CTRL )
            {
                switch ( Fl::event_key() )
                {
                    case FL_Delete:
                        delete_time();
                        break;
                    case FL_Insert:
                        insert_time();
                        break;
                    case FL_Left:
                        move_selected( LEFT, 1 );
                        break;
                    case FL_Right:
                        move_selected( RIGHT, 1 );
                        break;
                    case FL_Up:
                        move_selected( UP, 1 );
                        break;
                    case FL_Down:
                        move_selected( DOWN, 1 );
                        break;
                    default:
                        return Fl_Group::handle( ev );
                }

                return 1;
            }
            else if ( Fl::event_state() & FL_ALT )
            {
                switch ( Fl::event_key() )
                {
                    case FL_Right:
                        pan( TO_NEXT_NOTE, 0 );
                        break;
                    case FL_Left:
                        pan( TO_PREV_NOTE, 0 );
                        break;
                    default:
                        return Fl_Group::handle( ev );
                }

                return 1;
            }

            switch ( Fl::event_key() )
            {
                case FL_Left:
                    pan( LEFT, 1 );
                    break;
                case FL_Right:
                    pan( RIGHT, 1 );
                    break;
                case FL_Up:
                    pan( UP, 1 );
                    break;
                case FL_Down:
                    pan( DOWN, 1 );
                    break;
                default:
                    /* have to do this to get shifted keys */
                    switch ( *Fl::event_text() )
                    {
                        case 'f':
                            pan( TO_PLAYHEAD, 0 );
                            break;
                        case 'C':
                            crop();
                            break;
                        case 'd':
                        {
                            grid( grid()->clone() );
                            ui->update_sequence_widgets();              // number of phrases may have changed.
                            break;
                        }
                        case 'D':
                            duplicate_range();
                            break;
                        case 't':
                            grid()->trim();
                            break;
                        default:
                            return Fl_Group::handle( ev );
                    }
            }

            return 1;
        }

        case FL_PUSH:           // Mouse button press
        {
            Fl::focus(this);

            if ( Fl::event_button() == 1 )      // Left mouse button press?
            {
                int dx = x;
                int dy = y;
                    
                grid_pos( &dx, &dy );

                if ( is_ruler_click() )         // Click on ruler - range select
                {
                    _event_state = EVENT_STATE_SELECT_RANGE;
                    set_selection( dx, dx, 0, 128 );
                    return 1;
                }

                int note;
                if ( ( note = is_row_press() ) >= 0 )        // Row name click?
                {
                    if ( IS_PATTERN )
                        ((pattern *)grid())->row_name_press( note );

                    return 1;                        
                }

                // Click not on note area?
                if ( ! Fl::event_inside( this->x() + this->m.margin_left,
                                         this->y() + this->m.margin_top,
                                         this->w() - this->m.margin_left,
                                         ( this->h() - this->m.margin_top ) - this->panzoomer->h() ) )
                    break;

                bool note_clicked = this->m.grid->is_set( dx,dy );

                if ( note_clicked )
                    this->m.grid->get_note_properties( dx, dy, &drag_note );

                if ( Fl::event_alt() )          // ALT click? - Note velocity/duration change or delete
                {   // Return if SHIFT or CTRL also pressed or not a note click
                    if ( Fl::event_shift() || Fl::event_ctrl() || ! note_clicked )
                        return 1;

                    m.grid->select_none();
                    m.grid->toggle_select( dx, dy );

                    _event_state = EVENT_STATE_ALT;
                    hdrag_threshold = false;
                    vdrag_threshold = false;
                    drag_velocity = drag_note.velocity;
                    drag_x = x;
                    drag_y = y;

                    return 1;
                }

                if ( Fl::event_ctrl() )         // CTRL click? - rectangular selection
                {
                    _event_state = EVENT_STATE_SELECT_RECTANGLE;
                    set_selection( dx, dx, dy, rtn( ntr( dy ) + 1 ) );
                    hdrag_threshold = false;
                    vdrag_threshold = false;
                    drag_x = x;
                    drag_y = y;

                    signal_settings_change();

                    return 1;
                }

                // No note at clicked position?
                if ( ! note_clicked )
                {
                    m.grid->select_none();

                    m.grid->set_undo_group ("move");    // Group new notes with any drag moves

                    // Add new note, select it exclusively and enable move mode
                    this->m.grid->put( dx, dy,
                                       this->m.grid->default_length(),
                                       this->m.grid->default_velocity() );

                    m.grid->toggle_select( dx, dy );

                    _event_state = EVENT_STATE_MOVE;
                    last_move_x = dx;
                    last_move_y = ntr( dy );

                    return 1;
                }

                // Note found at clicked position

                if ( ! drag_note.selected )            // Note is not selected?
                {   // Select the note exclusively
                    m.grid->select_none();
                    m.grid->toggle_select( dx, dy );
                }

                // Activate move mode
                _event_state = EVENT_STATE_MOVE;
                last_move_x = dx;
                last_move_y = ntr( dy );
                drag_x = x;
                drag_y = y;

                take_focus();

                return 1;
            }
            else if ( Fl::event_button() == 3 )      // Right mouse button press?
            {
                int note;
                if ( ( note = is_row_press() ) >= 0 )   // Click is inside the note row headings?
                {
                    if ( IS_PATTERN )
                    {
                        Instrument *i = ((pattern *)grid())->mapping.instrument();

                        if ( i )
                        {
                            ui->edit_instrument_row( i, note );
                            changed_mapping();
                        }
                    }

                    return 1;
                }
            }

            break;
        }

        case FL_DRAG:           // Mouse drag
        {
            if ( Fl::event_is_click() )
                return 1;

            int dx = x;
            int dy = y;

            grid_pos( &dx, &dy );

            // Selection mode?
            if ( _event_state == EVENT_STATE_SELECT_RECTANGLE
                 || _event_state == EVENT_STATE_SELECT_RANGE )
            {   // Has threshold not yet been exceeded?
                if ( !vdrag_threshold && abs( drag_y - y ) >= DRAG_THRESHOLD )
                    vdrag_threshold = true;
                if ( !hdrag_threshold && abs( drag_x - x ) >= DRAG_THRESHOLD )
                    hdrag_threshold = true;
                if ( !vdrag_threshold && !hdrag_threshold )
                    return 1;

                if ( dx >= _selection.x1 ) dx++;

                if ( _event_state == EVENT_STATE_SELECT_RANGE )
                {
                    if ( dx != _selection.x2 )
                    {
                        _selection.x2 = dx;
                        damage(FL_DAMAGE_OVERLAY);
                    }
                }
                else
                {
                    if ( dy >= _selection.y1 ) dy = rtn( ntr( dy ) + 1 );

                    if ( dx != _selection.x2 || dy != _selection.y2 )
                    {
                        _selection.x2 = dx;
                        _selection.y2 = dy;
                        damage(FL_DAMAGE_OVERLAY);
                    }
                }

                return 1;
            }

            // Move mode?
            if ( _event_state == EVENT_STATE_MOVE )
            {
                int odx = drag_x;
                int ody = drag_y;
                grid_pos( &odx, &ody );

                if ( last_move_x != dx )
                {
                    if ( dx > last_move_x )
                        move_selected( RIGHT, dx - last_move_x );
                    else
                        move_selected( LEFT, last_move_x - dx );

                    last_move_x = dx;
                }

                if ( dy >= 0 && dy <= 127 )
                {
                    dy = ntr( dy );

                    if ( dy > last_move_y  )
                        move_selected( DOWN, dy - last_move_y );
                    else if ( dy < last_move_y )
                        move_selected( UP, last_move_y - dy );

                    last_move_y = dy;
                }

                return 1;
            }
      
            // Alternate mode? (Change velocity/duration or delete)
            if ( _event_state == EVENT_STATE_ALT )
            {
                int ody = drag_y;
                int odx = drag_x;
                    
                grid_pos( &odx, &ody );

                // Horizontal drag threshold already or just now met? (exclusive vertical or horizontal drag)
                if ( !vdrag_threshold && ( hdrag_threshold || abs( drag_x - x ) >= DRAG_THRESHOLD ) )
                {
                    hdrag_threshold = true;

                    if ( dx < 0 ) dx = 0;

                    int duration = dx - m.grid->ts_to_x( drag_note.start );

                    if ( duration < 1 )
                        duration = 1;

                    if ( duration != drag_note.duration )
                    {
                        drag_note.duration = duration;          // We store duration in grid units (not ticks)
                        m.grid->set_undo_group ("duration");
                        m.grid->set_sel_duration ( duration );
                    }
                }

                // Vertical drag threshold already or just now met? (exclusive vertical or horizontal drag)
                if ( !hdrag_threshold && ( vdrag_threshold || abs( drag_y - y ) >= DRAG_THRESHOLD ) )
                {
                    int velocity = drag_velocity + drag_y - y;

                    vdrag_threshold = true;

                    if ( velocity < 1 ) velocity = 1;
                    else if ( velocity > 127 ) velocity = 127;

                    if ( velocity != drag_note.velocity )
                    {
                        drag_note.velocity = velocity;
                        m.grid->set_undo_group ("velocity");
                        m.grid->set_sel_velocity( velocity );
                    }
                }

                return 1;
            }
            break;
        }

        case FL_RELEASE:                // Mouse button release
        {
            int ody = drag_y;
            int odx = drag_x;

            grid_pos( &odx, &ody );

            if ( _event_state == EVENT_STATE_SELECT_RANGE
                 || _event_state == EVENT_STATE_SELECT_RECTANGLE )
            {   // If threshold not exceeded, toggle note selection
                if ( !hdrag_threshold && !vdrag_threshold )
                {
                    set_selection( 0, 0, 0, 0 );
                    m.grid->toggle_select( odx, ody );
                }
                else
                {
                    fix_selection();        // Normalize the selection

                    // Shift adds to selection
                    if ( ! Fl::event_shift() )
                        grid()->select_none();

                    // Check for zero sized selection
                    if ( _selection.x1 != _selection.x2
                         || ( _event_state == EVENT_STATE_SELECT_RECTANGLE && _selection.y1 != _selection.y2 ) )
                    {
                        if ( _event_state == EVENT_STATE_SELECT_RANGE ) select_range();
                        else grid()->select( _selection.x1, _selection.x2, _selection.y1, _selection.y2 );
                    }
                    else set_selection( 0, 0, 0, 0 );
                }

                _event_state = EVENT_STATE_NONE;
                damage(FL_DAMAGE_OVERLAY);
                return 1;
            }
            else if ( _event_state == EVENT_STATE_ALT )
            {   // If drag not exceeded, then we have a delete
                if ( ! ( hdrag_threshold || vdrag_threshold ) )
                    m.grid->del( odx, ody );

                m.grid->reset_undo_group();
                _event_state = EVENT_STATE_NONE;
                return 1;
            }
            else if ( _event_state == EVENT_STATE_MOVE )
            {
                m.grid->reset_undo_group();
                _event_state = EVENT_STATE_NONE;
                return 1;
            }

            break;
        }

        default:
            break;
    }

    return Fl_Group::handle( ev );
}
