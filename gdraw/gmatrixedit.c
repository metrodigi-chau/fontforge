/* Copyright (C) 2006 by George Williams */
/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.

 * The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "gdraw.h"
#include "gkeysym.h"
#include "ggadgetP.h"
#include "gwidget.h"
#include <string.h>
#include <ustring.h>

#define DEL_SPACE	6

static GBox gmatrixedit_box = { /* Don't initialize here */ 0 };
static FontInstance *gmatrixedit_font = NULL, *gmatrixedit_titfont = NULL;
static int gmatrixedit_inited = false;

static void _GMatrixEdit_Init(void) {
    FontRequest rq;

    if ( gmatrixedit_inited )
return;
    _GGadgetCopyDefaultBox(&gmatrixedit_box);
    gmatrixedit_box.border_type = bt_none;
    gmatrixedit_box.border_width = 0;
    gmatrixedit_box.border_shape = bs_rect;
    gmatrixedit_box.padding = 0;
    gmatrixedit_box.flags = 0;
    gmatrixedit_box.main_background = COLOR_TRANSPARENT;
    gmatrixedit_box.disabled_background = COLOR_TRANSPARENT;
    GDrawDecomposeFont(_ggadget_default_font,&rq);
    gmatrixedit_font = GDrawInstanciateFont(screen_display,&rq);
    gmatrixedit_font = _GGadgetInitDefaultBox("GMatrixEdit.",&gmatrixedit_box,gmatrixedit_font);
    GDrawDecomposeFont(gmatrixedit_font,&rq);
    rq.point_size = (rq.point_size>=12) ? rq.point_size-2 : rq.point_size>=10 ? rq.point_size-1 : rq.point_size;
    rq.weight = 700; 
    gmatrixedit_titfont = GDrawInstanciateFont(screen_display,&rq);
    gmatrixedit_inited = true;
}

static void MatrixDataFree(GMatrixEdit *gme) {
    int r,c;

    for ( r=0; r<gme->rows; ++r ) for ( c=0; c<gme->cols; ++c ) {
	if ( gme->col_data[c].me_type == me_string ||
		gme->col_data[c].me_type == me_bigstr ||
		gme->col_data[c].me_type == me_func )
	    free( gme->data[r*gme->cols+c].u.md_str );
    }
    free( gme->data );
}

static void GMatrixEdit_destroy(GGadget *g) {
    GMatrixEdit *gme = (GMatrixEdit *) g;
    int c;

    /* The textfield lives in the nested window and doesn't need to be destroyed */
    /* if ( gme->tf!=NULL ) */
	/* GGadgetDestroy(gme->tf);*/
    if ( gme->vsb!=NULL )
	GGadgetDestroy(gme->vsb);
    if ( gme->hsb!=NULL )
	GGadgetDestroy(gme->hsb);
    if ( gme->del!=NULL )
	GGadgetDestroy(gme->del);
    if ( gme->nested!=NULL ) {
	GDrawSetUserData(gme->nested,NULL);
	GDrawDestroyWindow(gme->nested);
    }

    MatrixDataFree(gme);	/* Uses col data */

    for ( c=0; c<gme->cols; ++c ) {
	if ( gme->col_data[c].enum_vals!=NULL )
	    GMenuItemArrayFree(gme->col_data[c].enum_vals);
	free( gme->col_data[c].title );
    }
    free( gme->col_data );

    _ggadget_destroy(g);
}

static void GME_FixScrollBars(GMatrixEdit *gme) {
    int width;

    GScrollBarSetBounds(gme->vsb,0,gme->rows+1,gme->vsb->r.height/(gme->fh+gme->vpad));
    width = gme->col_data[gme->cols-1].x + gme->col_data[gme->cols-1].width;
    GScrollBarSetBounds(gme->hsb,0,width,gme->hsb->r.width);
}

static void GMatrixEdit_Move(GGadget *g, int32 x, int32 y) {
    GMatrixEdit *gme = (GMatrixEdit *) g;

    /* I don't need to move the textfieldbecause they is */
    /* nested within the window, and I'm moving the window */
    if ( gme->vsb!=NULL )
	_ggadget_move((GGadget *) (gme->vsb),x+(gme->vsb->r.x-g->r.x),
					     y+(gme->vsb->r.y-g->r.y));
    if ( gme->hsb!=NULL )
	_ggadget_move((GGadget *) (gme->hsb),x+(gme->hsb->r.x-g->r.x),
					     y+(gme->hsb->r.y-g->r.y));
    if ( gme->del!=NULL )
	_ggadget_move((GGadget *) (gme->del),x+(gme->del->r.x-g->r.x),
					     y+(gme->del->r.y-g->r.y));
    GDrawMove(gme->nested,x+(g->inner.x-g->r.x),y+(g->inner.y-g->r.y+
	    (gme->has_titles?gme->fh:0)));
    _ggadget_move(g,x,y);
}

static int GME_ColWidth(GMatrixEdit *gme, int c) {
    int width=0, max, cur;
    FontInstance *old = GDrawSetFont(gme->g.base,gme->font);
    char *str, *pt;
    int i, r;

    switch ( gme->col_data[c].me_type ) {
      case me_int:
	width = GDrawGetText8Width(gme->g.base,"123456", -1, NULL );
      break;
      case me_real:
	width = GDrawGetText8Width(gme->g.base,"1.234567", -1, NULL );
      break;
      case me_enum:
	max = 0;
	if ( gme->col_data[c].enum_vals!=NULL ) {
	    GMenuItem *mi = gme->col_data[c].enum_vals;
	    for ( i=0; mi[i].ti.text!=NULL || mi[i].ti.line ; ++i ) {
		if ( mi[i].ti.text!=NULL ) {
		    cur = GDrawGetTextWidth(gme->g.base,mi[i].ti.text, -1, NULL );
		    if ( cur>max ) max = cur;
		}
	    }
	}
	cur = 6 * GDrawGetText8Width(gme->g.base,"n", 1, NULL );
	if ( max<cur )
	    max = cur;
	width = max;
      break;
      case me_func:
      case me_string: case me_bigstr:
	max = 0;
	for ( r=0; r<gme->rows; ++r ) {
	    char *freeme = NULL;
	    str = gme->data[r*gme->cols+c].u.md_str;
	    if ( str==NULL && gme->col_data[c].me_type==me_func )
		str = freeme = (gme->col_data[c].func)(&gme->g,r,c);
	    pt = strchr(str,'\n');
	    cur = GDrawGetText8Width(gme->g.base,str, pt==NULL ? -1: pt-str, NULL );
	    if ( cur>max ) max = cur;
	    free(freeme);
	}
	if ( max < 10*GDrawGetText8Width(gme->g.base,"n", 1, NULL ) )
	    width = 10*GDrawGetText8Width(gme->g.base,"n", 1, NULL );
	else
	    width = max;
      break;
      default:
	width = 0;
      break;
    }
    if ( gme->col_data[c].title!=NULL ) {
	GDrawSetFont(gme->g.base,gme->titfont);
	cur = GDrawGetText8Width(gme->g.base,gme->col_data[c].title, -1, NULL );
	if ( cur>width ) width = cur;
    }
    GDrawSetFont(gme->g.base,old);
return( width );
}

static void GME_RedrawTitles(GMatrixEdit *gme);
static void GME_AdjustCol(GMatrixEdit *gme,int col) {
    int new_width, x,c;

    if ( !gme->col_data[col].fixed ) {
	new_width = GME_ColWidth(gme,col);
	if ( new_width!=gme->col_data[col].width ) {
	    gme->col_data[col].width = new_width;
	    x = gme->col_data[col].x;
	    for ( c=col; c<gme->cols; ++c ) {
		gme->col_data[c].x = x;
		x += gme->col_data[c].width + gme->hpad;
	    }
	    GME_FixScrollBars(gme);
	    GDrawRequestExpose(gme->nested,NULL,false);
	    GME_RedrawTitles(gme);
	}
    }
}

static void GMatrixEdit_GetDesiredSize(GGadget *g,GRect *outer,GRect *inner) {
    GMatrixEdit *gme = (GMatrixEdit *) g;
    int width, height;
    int bp = GBoxBorderWidth(g->base,g->box);
    int c, rows;
    FontInstance *old = GDrawSetFont(gme->g.base,gme->font);
    int sbwidth = GDrawPointsToPixels(g->base,_GScrollBar_Width);

    width = 1;
    for ( c=0; c<gme->cols; ++c ) {
	width += GME_ColWidth(gme,c);
	if ( c!=gme->cols-1 )
	    width += gme->hpad;
    }
    GDrawSetFont(gme->g.base,old);
    width += sbwidth;

    rows = (gme->rows<4) ? 4 : (gme->rows>20) ? 21 : gme->rows+1;
    height = rows * (gme->fh + gme->vpad);

    if ( gme->has_titles )
	height += gme->fh;
    height += sbwidth;
    if ( gme->del )
	height += gme->del->r.height+DEL_SPACE;

    if ( inner!=NULL ) {
	inner->x = inner->y = 0;
	inner->width = width;
	inner->height = height;
    }
    if ( outer!=NULL ) {
	outer->x = outer->y = 0;
	outer->width = width + 2*bp;
	outer->height = height + 2*bp;
    }
}

static void GME_PositionEdit(GMatrixEdit *gme);
static void GMatrixEdit_Resize(GGadget *g, int32 width, int32 height ) {
    GMatrixEdit *gme = (GMatrixEdit *) g;
    int bp = GBoxBorderWidth(g->base,g->box);
    int subheight, subwidth;
    /*int plus, extra,x,c;*/

    width -= 2*bp; height -= 2*bp;

    subheight = height - (gme->del->r.height+DEL_SPACE) -
	    (gme->has_titles?gme->fh:0) -
	    gme->hsb->r.height;
    subwidth = width - gme->vsb->r.width;
    GDrawResize(gme->nested,subwidth, subheight);
    GGadgetResize(gme->vsb,gme->vsb->r.width,subheight);
    GGadgetMove(gme->vsb,gme->g.inner.x + width-2*bp-gme->vsb->r.width,
			    gme->vsb->r.y);
    GGadgetResize(gme->hsb,subwidth,gme->hsb->r.height);
    GGadgetMove(gme->hsb,gme->g.inner.x,
			 gme->g.inner.y + height - (gme->del->r.height+DEL_SPACE) - gme->hsb->r.height);
    GME_FixScrollBars(gme);
    GGadgetMove(gme->del,gme->g.inner.x + (width-gme->del->r.width)/2,
			     gme->g.inner.y + height - (gme->del->r.height+DEL_SPACE/2));

#if 0		/* Leave columns as they are. scrollbar will work */
    plus = (width-gme->g.inner.width)/gme->cols;
    extra = (width-gme->g.inner.width) - plus*gme->cols;
    if ( extra<0 ) {
	extra += gme->cols;
	--plus;
    }

    x = 1;
    for ( c=0; c<gme->cols; ++c ) {
	int subwidth = gme->col_data[c].width+plus + (extra>0);
	--extra;
	if ( gme->edit_active && gme->active_col == c ) {
	    int y = (gme->active_row-gme->off_top)*(gme->fh+gme->vpad);
	    GGadget *g = gme->tf;
	    GGadgetMove(g,gme->g.inner.x+x,y);
	    GGadgetResize(g,subwidth,gme->fh);
	}
	gme->col_data[c].x = x;
	gme->col_data[c].width = subwidth;
	x += subwidth + gme->hpad;
    }
#endif
    _ggadget_resize(g,width+2*bp, height+2*bp);
    GME_PositionEdit(gme);
}

static int GMatrixEdit_Mouse(GGadget *g, GEvent *event) {
    GMatrixEdit *gme = (GMatrixEdit *) g;
    int c, nw, i, x;

    if ( gme->pressed_col>=0 && (event->type==et_mouseup || event->type==et_mousemove)) {
	c = gme->pressed_col;
	nw = (event->u.mouse.x-gme->g.inner.x-gme->col_data[c].x-gme->hpad/2);
	if ( event->u.mouse.x-gme->g.inner.x < gme->col_data[c].x ) {
	    if ( nw<=0 )
		nw = 1;
	}
	nw = (event->u.mouse.x-gme->g.inner.x-gme->col_data[c].x-gme->hpad/2);
	x = gme->col_data[c].x;
	for ( i=c; i<gme->cols; ++i ) {
	    gme->col_data[i].x = x;
	    x += gme->col_data[i].width + gme->hpad;
	}
	gme->col_data[c].width = nw;
	if ( event->type==et_mouseup )
	    GME_FixScrollBars(gme);
	GME_RedrawTitles(gme);
	GME_PositionEdit(gme);
	GDrawRequestExpose(gme->nested,NULL,false);
	if ( event->type==et_mouseup ) {
	    GDrawSetCursor(g->base,ct_pointer);
	    gme->pressed_col = -1;
	}
return( true );
    }

    if ( !gme->has_titles ||
	    event->u.mouse.x< gme->hsb->r.x || event->u.mouse.x >= gme->hsb->r.x+gme->hsb->r.width ||
	    event->u.mouse.y< gme->g.inner.y || event->u.mouse.y>=gme->g.inner.y+gme->fh ) {
	if ( gme->lr_pointer ) {
	    gme->lr_pointer = false;
	    GDrawSetCursor(g->base,ct_pointer);
	}
return( false );
    }
    for ( c=0; c<gme->cols; ++c ) {
	if ( event->u.mouse.x>= gme->g.inner.x + gme->col_data[c].x+gme->col_data[c].width+gme->hpad/2-4 &&
		event->u.mouse.x<= gme->g.inner.x + gme->col_data[c].x+gme->col_data[c].width+gme->hpad/2+4 )
    break;
    }
    if ( c==gme->cols ) {
	if ( gme->lr_pointer ) {
	    gme->lr_pointer = false;
	    GDrawSetCursor(g->base,ct_pointer);
	}
    } else {
	if ( !gme->lr_pointer ) {
	    gme->lr_pointer = true;
	    GDrawSetCursor(g->base,ct_4way);
	}
	if ( event->type == et_mousedown )
	    gme->pressed_col = c;
    }
return( true );
}

static int GMatrixEdit_Expose(GWindow pixmap, GGadget *g, GEvent *event) {
    GMatrixEdit *gme = (GMatrixEdit *) g;
    GRect r, old, older;
    int c, y;

    if ( gme->has_titles ) {
	r = gme->g.inner;
	r.height = gme->fh;
	r.width = gme->hsb->r.width;
	GDrawPushClip(pixmap,&r,&older);
	GDrawFillRect(pixmap,&r,0x808080);
	y = r.y + gme->as;
	GDrawSetFont(pixmap,gme->titfont);
	for ( c=0; c<gme->cols; ++c ) if ( gme->col_data[c].title!=NULL ) {
	    r.x = gme->col_data[c].x + gme->g.inner.x;
	    r.width = gme->col_data[c].width;
	    GDrawPushClip(pixmap,&r,&old);
	    GDrawDrawText8(pixmap,r.x,y,gme->col_data[c].title,-1,NULL,0x000000);
	    GDrawPopClip(pixmap,&old);
	    if ( c!=gme->cols-1 )
		GDrawDrawLine(pixmap,r.x+gme->col_data[c].width+gme->hpad/2,r.y,
				     r.x+gme->col_data[c].width+gme->hpad/2,r.y+r.height,
				     0xffffff);
	}
	GDrawPopClip(pixmap,&older);
    }
return( true );
}

static void GME_RedrawTitles(GMatrixEdit *gme) {
    GMatrixEdit_Expose(gme->g.base,&gme->g,NULL);
}

static void GMatrixEdit_SetVisible(GGadget *g, int visible ) {
    GMatrixEdit *gme = (GMatrixEdit *) g;
    if ( gme->vsb!=NULL ) _ggadget_setvisible(gme->vsb,visible);
    if ( gme->del!=NULL ) _ggadget_setvisible(gme->del,visible);
    GDrawSetVisible(gme->nested,visible);
    _ggadget_setvisible(g,visible);
}

static void GMatrixEdit_SetFont(GGadget *g,FontInstance *new) {
    GMatrixEdit *gme = (GMatrixEdit *) g;
    int as, ds, ld;
    gme->font = new;
    GDrawFontMetrics(gme->font,&as, &ds, &ld);
    gme->as = as;
    gme->fh = as+ds;
    GME_FixScrollBars(gme);
    GDrawRequestExpose(gme->nested,NULL,false);
}

static FontInstance *GMatrixEdit_GetFont(GGadget *g) {
    GMatrixEdit *gme = (GMatrixEdit *) g;
return( gme->font );
}

static int32 GMatrixEdit_IsSelected(GGadget *g, int32 pos) {
    GMatrixEdit *gme = (GMatrixEdit *) g;

return( pos==gme->active_row );
}

static int32 GMatrixEdit_GetFirst(GGadget *g) {
    GMatrixEdit *gme = (GMatrixEdit *) g;

return( gme->active_row );
}

static int GMatrixEdit_FillsWindow(GGadget *g) {
return( true );
}

struct gfuncs gmatrixedit_funcs = {
    0,
    sizeof(struct gfuncs),

    GMatrixEdit_Expose,
    GMatrixEdit_Mouse,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,

    _ggadget_redraw,
    GMatrixEdit_Move,
    GMatrixEdit_Resize,
    GMatrixEdit_SetVisible,
    _ggadget_setenabled,
    _ggadget_getsize,
    _ggadget_getinnersize,

    GMatrixEdit_destroy,

    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    GMatrixEdit_SetFont,
    GMatrixEdit_GetFont,

    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    GMatrixEdit_IsSelected,
    GMatrixEdit_GetFirst,
    NULL,
    NULL,
    NULL,

    GMatrixEdit_GetDesiredSize,
    NULL,
    GMatrixEdit_FillsWindow
};

static GMenuItem *FindMi(GMenuItem *mi, int val ) {
    int i;

    for ( i=0; mi[i].ti.text==NULL || mi[i].ti.line; ++i ) {
	if ( mi[i].ti.userdata == (void *) val && mi[i].ti.text!=NULL )
return( &mi[i] );
    }
return( NULL );
}

static void GME_PositionEdit(GMatrixEdit *gme) {
    int x,y,end;
    GRect wsize;
    int c = gme->active_col, r = gme->active_row;

    if ( gme->edit_active ) {
	x = gme->col_data[c].x - gme->off_left;
	y = (r-gme->off_top)*(gme->fh+gme->vpad);
	end = x + gme->col_data[c].width;

	GDrawGetSize(gme->nested,&wsize);
	if ( end>wsize.width )
	    end = wsize.width;
	GGadgetResize(gme->tf,end-x,gme->fh);
	GGadgetMove(gme->tf,x,y);
    }
}

static void GME_StrSmallEdit(GMatrixEdit *gme,char *str, GEvent *event) {

    gme->edit_active = true;
    /* Shift so all of column is in window???? */
    GME_PositionEdit(gme);
    GGadgetSetTitle8(gme->tf,str);
    GGadgetSetVisible(gme->tf,true);
    GGadgetSetEnabled(gme->tf,true);
    GWidgetIndicateFocusGadget(gme->tf);
    if ( event->type == et_mousedown )
	GGadgetDispatchEvent(gme->tf,event);
}

static int GME_SetValue(GMatrixEdit *gme,GGadget *g ) {
    int c = gme->active_col;
    int r = gme->active_row;
    int ival;
    double dval;
    char *end;
    char *str = GGadgetGetTitle8(g);

    switch ( gme->col_data[c].me_type ) {
      case me_enum:
	{
	    const unichar_t *ustr = _GGadgetGetTitle(g), *test;
	    int i;
	    for ( i=0; (test=gme->col_data[c].enum_vals[i].ti.text)!=NULL || gme->col_data[c].enum_vals[i].ti.line ; ++i ) {
		if ( u_strcmp(ustr,test)==0 ) {
		    gme->data[r*gme->cols+c].u.md_ival = (intpt) gme->col_data[c].enum_vals[i].ti.userdata;
		    free(str);
return( true );
		}
	    }
	}
	/* Didn't match any of the enums try as a direct integer */
      case me_int:
	ival = strtol(str,&end,10);
	if ( *end!='\0' ) {
	    GTextFieldSelect(g,end-str,-1);
	    free(str);
	    GDrawBeep(NULL);
return( false );
	}
	gme->data[r*gme->cols+c].u.md_ival = ival;
	free(str);
return( true );
      case me_real:
	dval = strtod(str,&end);
	if ( *end!='\0' ) {
	    GTextFieldSelect(g,end-str,-1);
	    free(str);
	    GDrawBeep(NULL);
return( false );
	}
	gme->data[r*gme->cols+c].u.md_real = dval;
	free(str);
return( true );
      case me_string: case me_bigstr: case me_func:
	free(gme->data[r*gme->cols+c].u.md_str);
	gme->data[r*gme->cols+c].u.md_str = str;
	/* Used to delete the row if this were a null string. seems extreme */
return( true );
      default:
	/* Eh? Can't happen */
	GTextFieldSelect(g,0,-1);
	GDrawBeep(NULL);
	free(str);
return( false );
    }
}

static int GME_FinishEdit(GMatrixEdit *gme) {

    if ( !gme->edit_active )
return( true );
    if ( !GME_SetValue(gme,gme->tf))
return( false );
    GGadgetSetVisible(gme->tf,false);
    gme->edit_active = false;
    GME_AdjustCol(gme,gme->active_col);

    gme->wasnew = false;
return( true );
}

static void GME_DeleteActive(GMatrixEdit *gme) {
    int r, c;

    if ( gme->active_row==-1 || (gme->candelete && !(gme->candelete)(&gme->g,gme->active_row))) {
	GDrawBeep(NULL);
return;
    }

    gme->edit_active = false;
    GGadgetSetVisible(gme->tf,false);
    for ( c=0; c<gme->cols; ++c ) {
	if ( gme->col_data[c].me_type == me_string || gme->col_data[c].me_type == me_bigstr )
	    free(gme->data[gme->active_row*gme->cols+c].u.md_str);
    }
    for ( r=gme->active_row+1; r<gme->rows; ++r )
	memcpy(gme->data+(r-1)*gme->cols,gme->data+r*gme->cols,
		gme->cols*sizeof(struct matrix_data));
    --gme->rows;
    gme->active_col = -1;
    if ( gme->active_row>=gme->rows ) gme->active_row = -1;
    GScrollBarSetBounds(gme->vsb,0,gme->rows,gme->vsb->inner.height/gme->fh);
    GDrawRequestExpose(gme->nested,NULL,false);
}

static void GME_EnableDelete(GMatrixEdit *gme) {
    int enabled = false;

    if ( gme->active_row>=0 && gme->active_row<gme->rows ) {
	enabled = true;
	if ( gme->candelete!=NULL && !(gme->candelete)(&gme->g,gme->active_row))
	    enabled = false;
    }
    GGadgetSetEnabled(gme->del,enabled);
}

#define CID_OK		1001
#define CID_Cancel	1002
#define CID_EntryField	1011

static int big_e_h(GWindow gw, GEvent *event) {
    GMatrixEdit *gme = GDrawGetUserData(gw);

    if ( event->type==et_close ) {
	gme->big_done = true;
    } else if ( event->type == et_char ) {
return( false );
    } else if ( event->type == et_controlevent && event->u.control.subtype == et_buttonactivate ) {
	gme->big_done = true;
	if ( GGadgetGetCid(event->u.control.g)==CID_OK ) {
	    gme->big_done = GME_SetValue(gme,GWidgetGetControl(gw,CID_EntryField) );
	    if ( gme->big_done )
		GME_AdjustCol(gme,gme->active_col);
	} else if ( gme->wasnew ) {
	    /* They canceled a click which produced a new row */
	    GME_DeleteActive(gme);
	    gme->wasnew = false;
	}
    }
return( true );
}

static void GME_StrBigEdit(GMatrixEdit *gme,char *str) {
    GRect pos;
    GWindow gw;
    GWindowAttrs wattrs;
    GGadgetCreateData mgcd[6], boxes[3], *varray[5], *harray[6];
    GTextInfo mlabel[6];
    char *title_str = NULL;

    if ( gme->bigedittitle!=NULL )
	title_str = (gme->bigedittitle)(&(gme->g),gme->active_row,gme->active_col);

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_utf8_wtitle|wam_undercursor|wam_isdlg|wam_restrict;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.is_dlg = true;
    wattrs.restrict_input_to_me = 1;
    wattrs.undercursor = 1;
    wattrs.cursor = ct_pointer;
    wattrs.utf8_window_title = title_str==NULL ? "Editing..." : title_str;
    pos.x = pos.y = 0;
    pos.width =GDrawPointsToPixels(NULL,GGadgetScale(200));
    pos.height = GDrawPointsToPixels(NULL,300);
    gme->big_done = 0;
    gw = GDrawCreateTopWindow(NULL,&pos,big_e_h,gme,&wattrs);
    free(title_str);

    memset(&mgcd,0,sizeof(mgcd));
    memset(&boxes,0,sizeof(boxes));
    memset(&mlabel,0,sizeof(mlabel));
    mgcd[0].gd.pos.x = 4; mgcd[0].gd.pos.y = 6;
    mgcd[0].gd.pos.width = 192;
    mgcd[0].gd.pos.height = 260;
    mgcd[0].gd.flags = gg_visible | gg_enabled | gg_textarea_wrap | gg_text_xim;
    mgcd[0].gd.cid = CID_EntryField;
    mgcd[0].creator = GTextAreaCreate;
    varray[0] = &mgcd[0]; varray[1] = NULL;
    varray[2] = &boxes[2]; varray[3] = NULL;
    varray[4] = NULL;

    mgcd[1].gd.pos.x = 30-3; mgcd[1].gd.pos.y = GDrawPixelsToPoints(NULL,pos.height)-35-3;
    mgcd[1].gd.pos.width = -1; mgcd[1].gd.pos.height = 0;
    mgcd[1].gd.flags = gg_visible | gg_enabled | gg_but_default;
    if ( _ggadget_use_gettext ) {
	mlabel[1].text = (unichar_t *) _("_OK");
	mlabel[1].text_is_1byte = true;
    } else
	mlabel[1].text = (unichar_t *) _STR_OK;
    mlabel[1].text_in_resource = true;
    mgcd[1].gd.label = &mlabel[1];
    mgcd[1].gd.cid = CID_OK;
    mgcd[1].creator = GButtonCreate;

    mgcd[2].gd.pos.x = -30; mgcd[2].gd.pos.y = mgcd[1].gd.pos.y+3;
    mgcd[2].gd.pos.width = -1; mgcd[2].gd.pos.height = 0;
    mgcd[2].gd.flags = gg_visible | gg_enabled | gg_but_cancel;
    if ( _ggadget_use_gettext ) {
	mlabel[2].text = (unichar_t *) _("_Cancel");
	mlabel[2].text_is_1byte = true;
    } else
	mlabel[2].text = (unichar_t *) _STR_Cancel;
    mlabel[2].text_in_resource = true;
    mgcd[2].gd.label = &mlabel[2];
    mgcd[2].gd.cid = CID_Cancel;
    mgcd[2].creator = GButtonCreate;
    harray[0] = GCD_Glue; harray[1] = &mgcd[1];
    harray[2] = GCD_Glue; harray[3] = &mgcd[2];
    harray[4] = GCD_Glue; harray[5] = NULL;

    boxes[2].gd.flags = gg_visible | gg_enabled;
    boxes[2].gd.u.boxelements = harray;
    boxes[2].creator = GHBoxCreate;

    boxes[0].gd.pos.x = boxes[0].gd.pos.y = 2;
    boxes[0].gd.flags = gg_visible | gg_enabled;
    boxes[0].gd.u.boxelements = varray;
    boxes[0].creator = GHVGroupCreate;

    GGadgetsCreate(gw,boxes);
    GHVBoxSetExpandableRow(boxes[0].ret,0);
    GHVBoxSetExpandableCol(boxes[2].ret,gb_expandgluesame);
    GHVBoxFitWindow(boxes[0].ret);
    GGadgetSetTitle8(mgcd[0].ret,str);

    GDrawSetVisible(gw,true);
    while ( !gme->big_done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(gw);
    GDrawRequestExpose(gme->nested,NULL,false);

    gme->wasnew = false;
}

static void GME_EnumDispatch(GWindow gw, GMenuItem *mi, GEvent *e) {
    GMatrixEdit *gme = GDrawGetUserData(gw);

    gme->data[gme->active_row*gme->cols+gme->active_col].u.md_ival = (intpt) mi->ti.userdata;
    GME_AdjustCol(gme,gme->active_col);
    gme->wasnew = false;
}

static void GME_Choices(GMatrixEdit *gme,GEvent *event,int r,int c) {
    GMenuItem *mi = gme->col_data[c].enum_vals;
    int val = gme->data[r*gme->cols+c].u.md_ival;
    int i;

    for ( i=0; mi[i].ti.text!=NULL || mi[i].ti.line || mi[i].ti.image!=NULL; ++i )
	mi[i].ti.selected = mi[i].ti.checked = (mi[i].ti.userdata == (void *) val);
    if ( gme->col_data[c].enable_enum!=NULL )
	(gme->col_data[c].enable_enum)(&gme->g,mi,r,c);
    GMenuCreatePopupMenu(gme->nested,event, mi);

    /* If wasnew is still set then they didn't pick anything, so remove the row */
    if ( gme->wasnew && c==0 )
	GME_DeleteActive(gme);
    gme->wasnew = false;
    GDrawRequestExpose(gme->nested,NULL,false);
}

static char *MD_Text(GMatrixEdit *gme,int r, int c ) {
    char buffer[20], *str= NULL;
    struct matrix_data *d = &gme->data[r*gme->cols+c];
    
    switch ( gme->col_data[c].me_type ) {
      case me_enum:
	/* Fall through into next case */
      case me_int:
	sprintf( buffer,"%d",d->u.md_ival );
	str = buffer;
      break;
      case me_real:
	sprintf( buffer,"%g",d->u.md_real );
	str = buffer;
      break;
      case me_string: case me_bigstr:
	str = d->u.md_str;
      break;
      case me_func:
	str = d->u.md_str;
	if ( str==NULL )
return( (gme->col_data[c].func)(&gme->g,r,c) );
      break;
    }
return( copy(str));
}

static void GMatrixEdit_StartSubGadgets(GMatrixEdit *gme,int r, int c,GEvent *event) {
    int i;
    struct matrix_data *d;

    /* new row */
    if ( c==0 && r==gme->rows && event->u.mouse.button==1 ) {
	if ( gme->rows>=gme->row_max )
	    gme->data = grealloc(gme->data,(gme->row_max+=10)*gme->cols*sizeof(struct matrix_data));
	++gme->rows;
	for ( i=0; i<gme->cols; ++i ) {
	    d = &gme->data[r*gme->cols+i];
	    memset(d,0,sizeof(*d));
	    switch ( gme->col_data[i].me_type ) {
	      case me_string: case me_bigstr:
		d->u.md_str = copy("");
	      break;
	      case me_enum:
		d->u.md_ival = (int) (intptr_t) gme->col_data[i].enum_vals[0].ti.userdata;
	      break;
	    }
	}
	if ( gme->initrow!=NULL )
	    (gme->initrow)(&gme->g,r);
	GME_FixScrollBars(gme);
	gme->wasnew = true;
    }

    if ( c==gme->cols || r>=gme->rows )
return;
    gme->active_col = c; gme->active_row = r;
    GME_EnableDelete(gme);

    d = &gme->data[r*gme->cols+c];
    if ( event->type==et_mousedown && event->u.mouse.button==3 ) {
	if ( gme->popupmenu!=NULL )
	    (gme->popupmenu)(&gme->g,event,r,c);
    } else if ( d->frozen ) {
	GDrawBeep(NULL);
    } else if ( gme->col_data[c].me_type==me_enum ) {
	GME_Choices(gme,event,r,c);
    } else {
	char *str = MD_Text(gme,gme->active_row,gme->active_col);
	if ( str==NULL )
return;
	else if ( str!=NULL &&
		(strlen(str)>40 || strchr(str,'\n')!=NULL || gme->col_data[c].me_type == me_bigstr))
	    GME_StrBigEdit(gme,str);
	else
	    GME_StrSmallEdit(gme,str,event);
	free(str);
    }
}

static void GMatrixEdit_SubGadgets(GMatrixEdit *gme,GEvent *event) {
    int r = event->u.mouse.y/(gme->fh+gme->vpad) + gme->off_top;
    int x = event->u.mouse.x + gme->off_left;
    int c;

    if ( gme->edit_active ) {
	if ( !GME_FinishEdit(gme) )
return;
    }
    for ( c=0; c<gme->cols; ++c ) {
	if ( x>=gme->col_data[c].x && x<=gme->col_data[c].x+gme->col_data[c].width )
    break;
    }
    GMatrixEdit_StartSubGadgets(gme,r,c,event);
}

static void GMatrixEdit_SubExpose(GMatrixEdit *gme,GWindow pixmap,GEvent *event) {
    int k, r,c;
    char buf[20];
    char *str, *freeme, *pt;
    GRect size;
    GRect clip, old;
    Color fg;
    struct matrix_data *data;
    GMenuItem *mi;

    GDrawGetSize(gme->nested,&size);

    GDrawDrawLine(pixmap,0,0,0,size.height,0x000000);
    for ( c=0; c<gme->cols-1; ++c )
	GDrawDrawLine(pixmap,
		gme->col_data[c].x+gme->col_data[c].width+gme->hpad/2-gme->off_left,0,
		gme->col_data[c].x+gme->col_data[c].width+gme->hpad/2-gme->off_left,size.height,
		0x000000);
    GDrawDrawLine(pixmap,0,0,size.width,0,0x000000);
    for ( r=gme->off_top; r<=gme->rows; ++r )
	GDrawDrawLine(pixmap,
		0,         (r-gme->off_top)*(gme->fh+gme->vpad)-1,
		size.width,(r-gme->off_top)*(gme->fh+gme->vpad)-1,
		0x000000);

    GDrawSetFont(pixmap,gme->font);
    for ( r=event->u.expose.rect.y/(gme->fh+gme->vpad);
	    r<=(event->u.expose.rect.y+event->u.expose.rect.height+gme->fh+gme->vpad-1)/gme->fh &&
	     r+gme->off_top<=gme->rows;
	    ++r ) {
	int y;
	clip.y = r*(gme->fh+gme->vpad);
	y = clip.y + gme->as;
	clip.height = gme->fh;
	if ( r+gme->off_top==gme->rows ) {
	    buf[0] = '<';
	    if ( _ggadget_use_gettext )
		strncpy(buf+1,_("New"),sizeof(buf)-2);
	    else
		u2utf8_strcpy(buf+1,GStringGetResource(_STR_New,NULL));
	    buf[18] = '\0';
	    k = strlen(buf);
	    buf[k] = '>'; buf[k+1] = '\0';
	    GDrawDrawText8(pixmap,gme->g.inner.x+gme->col_data[0].x - gme->off_left,y,
		    buf,-1,NULL,0x0000ff);
    break;
	}
	for ( c=0; c<gme->cols; ++c ) {
	    if ( gme->col_data[c].x + gme->col_data[c].width < gme->off_left )
	continue;
	    clip.x = gme->col_data[c].x-gme->off_left; clip.width = gme->col_data[c].width;
	    GDrawPushClip(pixmap,&clip,&old);
	    data = &gme->data[(r+gme->off_top)*gme->cols+c];
	    fg = data->frozen ? 0xff0000 : 0x000000;
	    str = freeme = NULL;
	    switch ( gme->col_data[c].me_type ) {
	      case me_enum:
		mi = FindMi(gme->col_data[c].enum_vals,data->u.md_ival);
		if ( mi!=NULL ) {
		    GDrawDrawText(pixmap,clip.x,y,mi->ti.text,-1,NULL,fg);
	    break;
		}
		/* Fall through into next case */
	      default:
	        str = MD_Text(gme,r+gme->off_top,c);
	      break;
	    }
	    if ( str!=NULL ) {
		pt = strchr(str,'\n');
		GDrawDrawText8(pixmap,clip.x,y,str,pt==NULL?-1:pt-str,NULL,fg);
		free(str);
	    }
	    GDrawPopClip(pixmap,&old);
	}
    }
}

static int matrixeditsub_e_h(GWindow gw, GEvent *event) {
    GMatrixEdit *gme = (GMatrixEdit *) GDrawGetUserData(gw);

    GGadgetPopupExternalEvent(event);
    if (( event->type==et_mouseup || event->type==et_mousedown ) &&
	    (event->u.mouse.button==4 || event->u.mouse.button==5) ) {
	if ( !(event->u.mouse.state&(ksm_shift|ksm_control)) )	/* bind shift to magnify/minify */
return( GGadgetDispatchEvent(gme->vsb,event));
    }

    switch ( event->type ) {
      case et_expose:
	GDrawSetLineWidth(gw,0);
	GMatrixEdit_SubExpose(gme,gw,event);
      break;
      case et_mousedown:
	GMatrixEdit_SubGadgets(gme,event);
      break;
      case et_mousemove:
	if ( gme->g.popup_msg!=NULL )
	    GGadgetPreparePopup(gw,gme->g.popup_msg);
      break;
      case et_mouseup:
      break;
      case et_char:
	switch ( event->u.chr.keysym ) {
	  case GK_Up: case GK_KP_Up:
	    if ( (!gme->edit_active || GME_FinishEdit(gme)) &&
		    gme->active_row>0 )
		GMatrixEdit_StartSubGadgets(gme,gme->active_row-1,gme->active_col,event);
	  break;
	  case GK_Down: case GK_KP_Down:
	    if ( (!gme->edit_active || GME_FinishEdit(gme)) &&
		    gme->active_row<gme->rows-(gme->active_col!=0) )
		GMatrixEdit_StartSubGadgets(gme,gme->active_row+1,gme->active_col,event);
	  break;
	  case GK_Left: case GK_KP_Left:
	  case GK_BackTab:
	  backtab:
	    if ( (!gme->edit_active || GME_FinishEdit(gme)) &&
		    gme->active_col>0 )
		GMatrixEdit_StartSubGadgets(gme,gme->active_row,gme->active_col-1,event);
	  break;
	  case GK_Tab:
	    if ( event->u.chr.state&ksm_shift )
	  goto backtab;
	    /* Else fall through */
	  case GK_Right: case GK_KP_Right:
	    if ( (!gme->edit_active || GME_FinishEdit(gme)) &&
		    gme->active_col<gme->cols-1 )
		GMatrixEdit_StartSubGadgets(gme,gme->active_row,gme->active_col+1,event);
	  break;
	  case GK_Return: case GK_KP_Enter:
	    if ( gme->edit_active )
		GME_FinishEdit(gme);
return( true );
	}
	if ( gme->handle_key!=NULL )
return( (gme->handle_key)(&gme->g,event) );
return( false );
      break;
      case et_destroy:
	if ( gme!=NULL )
	    gme->nested = NULL;
      break;
    }
return( true );
}

static void GME_HScroll(GMatrixEdit *gme,struct sbevent *sb) {
    int newpos = gme->off_left;
    GRect size;
    int hend = gme->col_data[gme->cols-1].x + gme->col_data[gme->cols-1].width;

    GDrawGetSize(gme->nested,&size);
    switch( sb->type ) {
      case et_sb_top:
        newpos = 0;
      break;
      case et_sb_uppage:
        newpos -= 9*size.width/10;
      break;
      case et_sb_up:
        newpos -= size.width/15;
      break;
      case et_sb_down:
        newpos += size.width/15;
      break;
      case et_sb_downpage:
        newpos += 9*size.width/10;
      break;
      case et_sb_bottom:
        newpos = hend;
      break;
      case et_sb_thumb:
      case et_sb_thumbrelease:
        newpos = sb->pos;
      break;
    }
    if ( newpos + size.width > hend )
	newpos = hend - size.width;
    if ( newpos<0 )
	newpos = 0;
    if ( newpos!=gme->off_left ) {
	int diff = gme->off_left-newpos;
	GRect r;
	gme->off_left = newpos;
	GScrollBarSetPos(gme->hsb,newpos);
	r.x = 1; r.y = 1; r.width = size.width-1; r.height = size.height-1;
	GDrawScroll(gme->nested,&r,diff,0);
	GME_PositionEdit(gme);
	GME_RedrawTitles(gme);
    }
}

static void GME_VScroll(GMatrixEdit *gme,struct sbevent *sb) {
    int newpos = gme->off_top;
    int page;
    GRect size;

    GDrawGetSize(gme->nested,&size);
    page = size.height/(gme->fh+gme->vpad);

    switch( sb->type ) {
      case et_sb_top:
        newpos = 0;
      break;
      case et_sb_uppage:
        newpos -= 9*page/10;
      break;
      case et_sb_up:
        newpos--;
      break;
      case et_sb_down:
        newpos++;
      break;
      case et_sb_downpage:
        newpos += 9*page/10;
      break;
      case et_sb_bottom:
        newpos = gme->rows+1;
      break;
      case et_sb_thumb:
      case et_sb_thumbrelease:
        newpos = sb->pos;
      break;
    }
    if ( newpos + page > gme->rows+1 )
	newpos = gme->rows+1 - page;
    if ( newpos<0 )
	newpos = 0;
    if ( newpos!=gme->off_top ) {
	int diff = (newpos-gme->off_top)*(gme->fh+gme->vpad);
	GRect r;
	gme->off_top = newpos;
	GScrollBarSetPos(gme->vsb,newpos);
	r.x = 1; r.y = 1; r.width = size.width-1; r.height = size.height-1;
	GDrawScroll(gme->nested,&r,0,diff);
	GME_PositionEdit(gme);
    }
}

static int _GME_HScroll(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_scrollbarchange ) {
	GMatrixEdit *gme = (GMatrixEdit *) g->data;
	GME_HScroll(gme,&e->u.control.u.sb);
    }
return( true );
}

static int _GME_VScroll(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_scrollbarchange ) {
	GMatrixEdit *gme = (GMatrixEdit *) g->data;
	GME_VScroll(gme,&e->u.control.u.sb);
    }
return( true );
}

static int _GME_DeleteActive(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	GMatrixEdit *gme = (GMatrixEdit *) g->data;
	GME_DeleteActive(gme);
    }
return( true );
}

static GMenuItem *GMenuItemFromTI(GTextInfo *ti) {
    int cnt;
    GMenuItem *mi;

    for ( cnt=0; ti[cnt].text!=NULL || ti[cnt].line; ++cnt );
    mi = gcalloc((cnt+1),sizeof(GMenuItem));
    for ( cnt=0; ti[cnt].text!=NULL || ti[cnt].line; ++cnt ) {
	mi[cnt].ti = ti[cnt];
	if ( mi[cnt].ti.text!=NULL ) {
	    if ( ti[cnt].text_is_1byte )
		mi[cnt].ti.text = (unichar_t *) copy( (char *) mi[cnt].ti.text );
	    else
		mi[cnt].ti.text = u_copy( mi[cnt].ti.text );
	    mi[cnt].ti.checkable = true;
	    mi[cnt].invoke = GME_EnumDispatch;
	}
    }
return( mi );
}

/* GMatrixElement: External interface *************************************** */
GGadget *GMatrixEditCreate(struct gwindow *base, GGadgetData *gd,void *data) {
    struct matrixinit *matrix = gd->u.matrix;
    GMatrixEdit *gme = gcalloc(1,sizeof(GMatrixEdit));
    int r, c, bp;
    int x;
    GRect outer;
    GRect pos;
    GWindowAttrs wattrs;
    int sbwidth = GDrawPointsToPixels(base,_GScrollBar_Width);
    GGadgetData sub_gd;
    GTextInfo label;
    int as, ds, ld;

    if ( !gmatrixedit_inited )
	_GMatrixEdit_Init();

    gme->g.funcs = &gmatrixedit_funcs;
    _GGadget_Create(&gme->g,base,gd,data,&gmatrixedit_box);
    gme->g.takes_input = true; gme->g.takes_keyboard = false; gme->g.focusable = false;

    gme->font = gmatrixedit_font;
    gme->titfont = gmatrixedit_titfont;
    GDrawFontMetrics(gme->font,&as, &ds, &ld);
    gme->fh = as+ds;
    gme->as = as;

    gme->rows = matrix->initial_row_cnt; gme->cols = matrix->col_cnt;
    gme->row_max = gme->rows;
    gme->hpad = gme->vpad = GDrawPointsToPixels(base,2);

    gme->col_data = galloc(gme->cols*sizeof(struct col_data));
    for ( c=0; c<gme->cols; ++c ) {
	gme->col_data[c].me_type = matrix->col_init[c].me_type;
	gme->col_data[c].func = matrix->col_init[c].func;
	if ( matrix->col_init[c].enum_vals!=NULL )
	    gme->col_data[c].enum_vals = GMenuItemFromTI(matrix->col_init[c].enum_vals);
	else
	    gme->col_data[c].enum_vals = NULL;
	gme->col_data[c].enable_enum = matrix->col_init[c].enable_enum;
	gme->col_data[c].title = copy( matrix->col_init[c].title );
	if ( gme->col_data[c].title!=NULL ) gme->has_titles = true;
	gme->col_data[c].fixed = false;
    }

    gme->data = gcalloc(gme->rows*gme->cols,sizeof(struct matrix_data));
    memcpy(gme->data,matrix->matrix_data,gme->rows*gme->cols*sizeof(struct matrix_data));
    for ( c=0; c<gme->cols; ++c ) {
	enum me_type me_type = gme->col_data[c].me_type;
	if ( me_type==me_string || me_type==me_bigstr || me_type==me_func ) {
	    for ( r=0; r<gme->rows; ++r )
		gme->data[r*gme->cols+c].u.md_str = copy(gme->data[r*gme->cols+c].u.md_str);
	}
    }

    /* Can't do this earlier. It depends on matrix_data being set */
    x = 1;
    for ( c=0; c<gme->cols; ++c ) {
	gme->col_data[c].x = x;
	gme->col_data[c].width = GME_ColWidth(gme,c);
	x += gme->col_data[c].width + gme->hpad;
    }

    gme->pressed_col = -1;
    gme->active_col = gme->active_row = -1;
    gme->initrow = matrix->initrow;
    gme->candelete = matrix->candelete;
    gme->popupmenu = matrix->popupmenu;
    gme->handle_key = matrix->handle_key;
    gme->bigedittitle = matrix->bigedittitle;

    GMatrixEdit_GetDesiredSize(&gme->g,&outer,NULL);
    if ( gme->g.r.width==0 )
	gme->g.r.width = outer.width;
    if ( gme->g.r.height==0 )
	gme->g.r.height = outer.height;
    bp = GBoxBorderWidth(gme->g.base,gme->g.box);
    gme->g.inner.x = gme->g.r.x + bp;
    gme->g.inner.y = gme->g.r.y + bp;
    gme->g.inner.width = gme->g.r.width -2*bp;
    gme->g.inner.height = gme->g.r.height -2*bp;

    memset(&sub_gd,0,sizeof(sub_gd));
    memset(&label,0,sizeof(label));
    sub_gd.pos.x = sub_gd.pos.y = 1; sub_gd.pos.width = sub_gd.pos.height = 0;
    label.text = (unichar_t *) _("Delete");
    label.text_is_1byte = true;
    sub_gd.flags = gg_visible | gg_enabled | gg_pos_in_pixels;
    sub_gd.label = &label;
    sub_gd.handle_controlevent = _GME_DeleteActive;
    gme->del = GButtonCreate(base,&sub_gd,gme);
    gme->del->contained = true;

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.cursor = ct_pointer;
    pos = gme->g.inner;
    pos.width -= sbwidth;
    pos.height -= sbwidth + gme->del->inner.height+DEL_SPACE;
    if ( gme->has_titles ) {
	pos.y += gme->fh;
	pos.height -= gme->fh;
    }
    gme->nested = GWidgetCreateSubWindow(base,&pos,matrixeditsub_e_h,gme,&wattrs);

    GGadgetMove(gme->del,
	    (gme->g.inner.width-gme->del->r.width)/2,
	    gme->g.inner.height-gme->del->r.height-DEL_SPACE/2);

    sub_gd.pos = pos;
    sub_gd.pos.x = pos.x+pos.width; sub_gd.pos.width = sbwidth;
    sub_gd.flags = gg_visible | gg_enabled | gg_sb_vert | gg_pos_in_pixels;
    sub_gd.handle_controlevent = _GME_VScroll;
    gme->vsb = GScrollBarCreate(base,&sub_gd,gme);
    gme->vsb->contained = true;

    sub_gd.pos = pos;
    sub_gd.pos.y = pos.y+pos.height; sub_gd.pos.height = sbwidth;
    sub_gd.flags = gg_visible | gg_enabled | gg_pos_in_pixels;
    sub_gd.handle_controlevent = _GME_HScroll;
    gme->hsb = GScrollBarCreate(base,&sub_gd,gme);
    gme->hsb->contained = true;

    {
	static GBox small = { 0 };
	static unichar_t nullstr[1] = { 0 };

	small.main_background = small.main_foreground = COLOR_DEFAULT;
	small.main_foreground = 0x0000ff;
	memset(&sub_gd,'\0',sizeof(sub_gd));
	memset(&label,'\0',sizeof(label));

	label.text = nullstr;
	label.font = gme->font;
	sub_gd.pos.height = gme->fh;
	sub_gd.pos.width = 40;
	sub_gd.label = &label;
	sub_gd.box = &small;
	sub_gd.flags = gg_enabled | gg_pos_in_pixels | gg_dontcopybox | gg_text_xim;
	gme->tf = GTextFieldCreate(gme->nested,&sub_gd,gme);
    }
    if ( gme->g.state!=gs_invisible )
	GDrawSetVisible(gme->nested,true);
return( &gme->g );
}

void GMatrixEditSet(GGadget *g,struct matrix_data *data, int rows, int copy_it) {
    GMatrixEdit *gme = (GMatrixEdit *) g;
    int r,c;

    MatrixDataFree(gme);

    gme->rows = gme->row_max = rows;
    if ( !copy_it ) {
	gme->data = data;
    } else {
	gme->data = gcalloc(rows*gme->cols,sizeof(struct matrix_data));
	memcpy(gme->data,data,rows*gme->cols*sizeof(struct matrix_data));
	for ( c=0; c<gme->cols; ++c ) {
	    enum me_type me_type = gme->col_data[c].me_type;
	    if ( me_type==me_string || me_type==me_bigstr || me_type==me_func ) {
		for ( r=0; r<rows; ++r )
		    gme->data[r*gme->cols+c].u.md_str = copy(gme->data[r*gme->cols+c].u.md_str);
	    }
	}
    }
}

void GMatrixEditDeleteRow(GGadget *g,int row) {
    GMatrixEdit *gme = (GMatrixEdit *) g;

    if ( row!=-1 )
	gme->active_row = row;
    GME_DeleteActive(gme);
}

int GMatrixEditStringDlg(GGadget *g,int row,int col) {
    GMatrixEdit *gme = (GMatrixEdit *) g;
    char *str;

    if ( gme->edit_active ) {
	if ( !GME_FinishEdit(gme) )
return(false);
    }
    if ( row!=-1 )
	gme->active_row = row;
    if ( col!=-1 )
	gme->active_col = col;
    str = MD_Text(gme,row,col);
    GME_StrBigEdit(gme,str);
    free(str);
return( true );
}

struct matrix_data *GMatrixEditGet(GGadget *g, int *rows) {
    GMatrixEdit *gme = (GMatrixEdit *) g;

    if ( gme->edit_active && !GME_FinishEdit(gme) ) {
	*rows = 0;
return( NULL );
    }
    
    *rows = gme->rows;
return( gme->data );
}