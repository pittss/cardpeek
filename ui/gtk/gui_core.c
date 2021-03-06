/**********************************************************************
*
* This file is part of Cardpeek, the smart card reader utility.
*
* Copyright 2009-2013 by Alain Pannetrat <L1L1@gmx.com>
*
* Cardpeek is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* Cardpeek is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Cardpeek.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#include <string.h>
#include "ui.h"
#include "gui_cardview.h"
#include "gui_readerview.h"
#include "gui_logview.h"
#include "gui_scratchpad.h"
#include "gui_inprogress.h"
#include "gui_about.h"
#include "misc.h"
#include "a_string.h"
#include "pathconfig.h"
#include "lua_ext.h"

#include "cardpeek_resources.gresource"

/*********************************************************/
/* THE INFAMOUS UGLY GLOBALS *****************************/
/*********************************************************/

/* #define DISPLAY_SUPPORTED_MIME_TYPES 1 */

GtkWidget *MAIN_WINDOW = NULL;
GtkWidget *MAIN_NOTEBOOK = NULL;

/*********************************************************/
/* GUI_* FUNCTIONS ***************************************/
/*********************************************************/

static int gui_question_l(const char *message, unsigned item_count, const char **items)
{
    GtkWidget *dialog;
    GtkWidget *label;
    GtkWidget *hbox;
    GtkWidget *img;
    int result;

    dialog = gtk_dialog_new_with_buttons ("Question",
                                          GTK_WINDOW(MAIN_WINDOW),
                                          GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                          NULL,
                                          NULL);
    for (result=0; result<(int)item_count; result++)
        gtk_dialog_add_button(GTK_DIALOG(dialog),items[result],result);

    img = gtk_image_new_from_icon_name("dialog-question", 
                                       GTK_ICON_SIZE_DIALOG);

    label = gtk_label_new(message);

    gtk_label_set_line_wrap(GTK_LABEL(label),TRUE);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);

    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 10);

    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 10);

    gtk_widget_show_all(hbox);

    gtk_container_add (GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       hbox);

    result = gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);

    if (result==GTK_RESPONSE_NONE)
        return -1;
    return result;
}

static int gui_readline(const char *message, unsigned input_max, char *input)
/* input_max does not include final '\0' */
{
    GtkWidget *dialog;
    GtkWidget *label;
    GtkWidget *hbox;
    GtkWidget *vbox;
    GtkWidget *img;
    GtkWidget *entry;
    gint result;

    if (input_max==0)
    {
        log_printf(LOG_ERROR,"'gui_readline' was called with maximum input length set to 0");
        return 0;
    }
    if (input==NULL)
    {
        log_printf(LOG_ERROR,"'gui_readline' was called with a NULL value");
        return 0;
    }

    dialog = gtk_dialog_new_with_buttons("Question",
                                         GTK_WINDOW(MAIN_WINDOW),
                                         GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                         "_OK",
                                         GTK_RESPONSE_ACCEPT,
                                         "_Cancel",
                                         GTK_RESPONSE_REJECT,
                                         NULL);

    img = gtk_image_new_from_icon_name("dialog-question",
                                       GTK_ICON_SIZE_DIALOG);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);

    label = gtk_label_new(message);

    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 10);

    entry = gtk_entry_new();

    gtk_entry_set_max_length(GTK_ENTRY(entry), input_max);

    gtk_entry_set_width_chars(GTK_ENTRY(entry),input_max<=80?input_max:80);

    gtk_entry_set_text(GTK_ENTRY(entry),input);

    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 10);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);

    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 10);

    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 10);

    gtk_widget_show_all(hbox);

    gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       hbox);

    result = gtk_dialog_run (GTK_DIALOG (dialog));

    if (result!=GTK_RESPONSE_ACCEPT)
    {
        input[0]=0;
        gtk_widget_destroy(dialog);
        return 0;
    }
    strncpy(input,gtk_entry_get_text(GTK_ENTRY(entry)),input_max);
    input[input_max]=0; /* this should not be needed */
    gtk_widget_destroy(dialog);
    return 1;
}

static char **gui_select_file(const char *title,
                       const char *path,
                       const char *filename)
{
    static char *ret_values[2];
    GtkWidget *filew;

    ret_values[0] = NULL;
    ret_values[1] = NULL;

    if (filename==NULL) /* open a file - filename==NULL */
    {
        filew = gtk_file_chooser_dialog_new (title,
                                             GTK_WINDOW(MAIN_WINDOW),
                                             GTK_FILE_CHOOSER_ACTION_OPEN,
                                             "_Cancel", GTK_RESPONSE_CANCEL,
                                             "_Open", GTK_RESPONSE_ACCEPT,
                                             NULL);
    }
    else /* save a file - filename contains the default name */
    {
        filew = gtk_file_chooser_dialog_new (title,
                                             GTK_WINDOW(MAIN_WINDOW),
                                             GTK_FILE_CHOOSER_ACTION_SAVE,
                                             "_Cancel", GTK_RESPONSE_CANCEL,
                                             "_Save", GTK_RESPONSE_ACCEPT,
                                             NULL);
        gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (filew), TRUE);
    }

    if (filew==NULL)
    { 
        log_printf(LOG_ERROR,"Could not load 'gtk file chooser', this is probably a local configuration issue.");
        return ret_values;
    }

    if (path)
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filew), path);
    else
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filew), path_config_get_string(PATH_CONFIG_FOLDER_HOME));

    if (filename)
        gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (filew), filename);

    if (gtk_dialog_run (GTK_DIALOG (filew)) == GTK_RESPONSE_ACCEPT)
    {
        ret_values[0] = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER (filew));
        ret_values[1] = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filew));
    }
    
    gtk_widget_destroy (filew);
    return ret_values;
}

static void gui_update()
{
    /*
    static unsigned ref=0;
    unsigned now = time(NULL);

    if (now-ref<1)
        return;
    ref = now;
    */
    while (gtk_events_pending ())
        gtk_main_iteration ();
}

static void gui_exit(void)
{
    /* FIXME: UGLY EXIT */
    log_close_file();
    gtk_main_quit();
}

static char *gui_select_reader(unsigned list_size, const char **list)
{
    GtkWidget *dialog;
    GtkWidget *label;
    GtkWidget *combo;
    GtkWidget *hbox;
    GtkWidget *vbox;
    GtkWidget *img;
    unsigned i;
    gint result;
    char *retval;


    dialog = gtk_dialog_new_with_buttons("Select card reader",
                                         GTK_WINDOW(MAIN_WINDOW),
                                         GTK_DIALOG_MODAL,
                                         "_OK",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);

    img = gtk_image_new_from_icon_name("dialog-question", 
                                       GTK_ICON_SIZE_DIALOG);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);

    label = gtk_label_new("Select a card reader to use");

    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 10);

    combo = gtk_combo_box_text_new();

    gtk_box_pack_start(GTK_BOX(vbox), combo, FALSE, FALSE, 10);

    for (i=0; i<list_size; i++)
    {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),list[i]);
    }
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),"none");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo),0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);

    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 10);

    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 10);

    gtk_widget_show_all(hbox);

    gtk_container_add (GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       hbox);

    result = gtk_dialog_run (GTK_DIALOG (dialog));

    switch (result)
    {
    case GTK_RESPONSE_ACCEPT:
        retval = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
        log_printf(LOG_INFO,"Selected '%s'",retval);
        if (strcmp(retval,"none")==0)
        {
            g_free(retval);
            retval = NULL;
        }
        break;
    default:
        retval = NULL;
        log_printf(LOG_INFO,"Select empty");
        break;
    }
    gtk_widget_destroy (dialog);
    return retval;
}

#ifdef DISPLAY_SUPPORTED_MIME_TYPES
static void gui_get_supported_image_mime_types(void)
{
    GSList *formats = gdk_pixbuf_get_formats();
    GdkPixbufFormat *format;
    a_string_t *info;
    char **mime_types;
    int i,total;

    info = a_strnew("");
    total=0;
    while (formats)
    {
        format = (GdkPixbufFormat *)formats->data;
        formats = formats->next;
        mime_types =  gdk_pixbuf_format_get_mime_types(format);
        for (i=0; mime_types[i]!=NULL; i++)
        {
            total++;
            if (((total+1)%5)==0)
                a_strcat(info,"\n\t ");
            else
                a_strcat(info," ");
            a_strcat(info,mime_types[i]);
        }
        g_strfreev(mime_types);
    }
    log_printf(LOG_DEBUG,"Supported image mime-types:%s",a_strval(info));
    g_slist_free(formats);
    a_strfree(info);
}
#endif


#if GTK_CHECK_VERSION(3,14,0)

static void add_icons_from_resource(const gchar *path)
{
    gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default(),path);
}

#else

static char *resource_basename(const char *r)
{
    static char name[128];
    const char *str_begin;
    const char *str_end;
    
    str_begin = strrchr(r,'/');
    if (str_begin==NULL)
        str_begin = r;
    else
        str_begin++;
    
    str_end = strchr(str_begin,'.');
    if (str_end==NULL)
        str_end = r+strlen(r);
    
    if (str_end-str_begin>127) 
        str_end = str_begin+127;
    
    memcpy(name,str_begin,str_end-str_begin);
    name[str_end-str_begin]=0;
    return name;
}

static void deprecated_icon_theme_add(GResource *resource, const gchar *path)
{
    char **children;
    int i;
    char *ext_path;
    GBytes *icon_bytes;
    unsigned char *icon_bytes_start;
    gsize icon_bytes_size;
    GdkPixbuf *pixbuf;
    GdkPixbuf *pixbuf_scaled;
    int real_icon_height;
    int icon_height;
    int icon_width;


    icon_bytes = g_resources_lookup_data(path,G_RESOURCE_LOOKUP_FLAGS_NONE,NULL);

    if (icon_bytes!=NULL)
    {
        icon_bytes_start = (unsigned char *)g_bytes_get_data(icon_bytes,&icon_bytes_size);

        pixbuf = gdk_pixbuf_new_from_inline (-1, icon_bytes_start, FALSE, NULL);

        real_icon_height = gdk_pixbuf_get_height(pixbuf);

        gtk_icon_theme_add_builtin_icon(resource_basename(path),
                                        real_icon_height,
                                        pixbuf);


        gtk_icon_size_lookup(GTK_ICON_SIZE_LARGE_TOOLBAR,&icon_width,&icon_height);

        pixbuf_scaled = gdk_pixbuf_scale_simple(pixbuf,icon_width,icon_height,GDK_INTERP_BILINEAR);
 
        gtk_icon_theme_add_builtin_icon(resource_basename(path),
                                        icon_height,
                                        pixbuf_scaled);
        
        icon_height = gdk_pixbuf_get_height(pixbuf_scaled);
        
        g_object_unref(pixbuf_scaled);
        g_object_unref(pixbuf);
        g_bytes_unref(icon_bytes);
    }
    else
    {
        children = g_resource_enumerate_children(resource, path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);

        if (children)
        {
            for (i=0;children[i]!=NULL;i++)
            {
                ext_path = g_malloc(strlen(path)+strlen(children[i])+2);
                sprintf(ext_path,"%s/%s",path,children[i]);
                deprecated_icon_theme_add(resource,ext_path);
                g_free(ext_path);
            }
            g_strfreev(children);
        }
    }
}

static void add_icons_from_resource(const gchar *path)
{
    GResource *cardpeek_resources = cardpeek_resources_get_resource();
    char *clean_path = g_strdup(path);
    
    if (strlen(clean_path)>0)
    {
        if (clean_path[strlen(clean_path)-1]=='/')
            clean_path[strlen(clean_path)-1]=0;
    }
    if (cardpeek_resources == NULL)
    {
        log_printf(LOG_ERROR,"Could not load cardpeek internal resources. This is not good.");
        return;
    }
    deprecated_icon_theme_add(cardpeek_resources,clean_path);
    g_free(clean_path);
}

#endif

static int gui_load_icons(void)
{
    GdkPixbuf *pixbuf;

    add_icons_from_resource("/com/pannetrat/cardpeek/icons/");

    pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "cardpeek-logo",48,0,NULL);
    if (pixbuf) {
        gtk_window_set_default_icon(pixbuf);
        g_object_unref(pixbuf);
        return 1;
    }
    /* This does not work:
     * gtk_window_set_default_icon_name("cardpeek-logo");
     */
    return 0;
}

static void notebook_on_destroy(GtkWidget *window, gpointer user_data)
{
    GtkWidget *page;
    GtkWidget *label;
    const char *label_text;
    GtkNotebook *notebook = GTK_NOTEBOOK(user_data);
    UNUSED(window);

    while ((page = gtk_notebook_get_nth_page(notebook,0)))
    {
        label_text = gtk_notebook_get_tab_label_text(notebook,page);
        label = gtk_label_new(label_text);
        gtk_widget_reparent(page, MAIN_NOTEBOOK);
        gtk_notebook_set_tab_label(GTK_NOTEBOOK(MAIN_NOTEBOOK),page,label);
        gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (MAIN_NOTEBOOK), page, TRUE);
        gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (MAIN_NOTEBOOK), page, TRUE);

    }
}


static void notebook_on_page_removed(GtkNotebook *notebook,
                                     GtkWidget   *child,
                                     guint        page_num,
                                     gpointer     user_data)
{
    GtkWidget *toplevel;
    UNUSED(child);
    UNUSED(page_num);
    UNUSED(user_data);

    if (gtk_notebook_get_n_pages(notebook)==0)
    {
        toplevel = gtk_widget_get_toplevel(GTK_WIDGET(notebook));
        gtk_widget_destroy(toplevel);
    }
}

static GtkNotebook *notebook_create_extra(GtkNotebook *source_notebook,
        GtkWidget   *child,
        gint         x,
        gint         y,
        gpointer     data)
{
    GtkWidget *window;
    GtkWidget *notebook;
    UNUSED(child);
    UNUSED(data);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    notebook = gtk_notebook_new ();


    g_signal_connect (notebook, "page-removed",
                      G_CALLBACK (notebook_on_page_removed), NULL);

    g_signal_connect (notebook, "create-window",
                      G_CALLBACK (notebook_create_extra), NULL);

    g_signal_connect (window, "destroy",
                      G_CALLBACK (notebook_on_destroy), notebook);

    gtk_notebook_set_group_name (GTK_NOTEBOOK (notebook),
                                 gtk_notebook_get_group_name (source_notebook));

    gtk_container_add (GTK_CONTAINER (window), notebook);

    gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
    gtk_window_move (GTK_WINDOW (window), x, y);
    gtk_widget_show_all (window);

    return GTK_NOTEBOOK (notebook);
}

static void internal_map_accel_group(GtkWidget *widget, gpointer userdata)
{
    GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
    GtkAccelGroup *accel_group = (GtkAccelGroup *)userdata;

    gtk_window_add_accel_group(GTK_WINDOW(toplevel), accel_group);
}

static void internal_unmap_accel_group(GtkWidget *widget, gpointer userdata)
{
    GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
    GtkAccelGroup *accel_group = (GtkAccelGroup *)userdata;

    gtk_window_remove_accel_group(GTK_WINDOW(toplevel), accel_group);
}

static int gui_create(void)
{
    GtkWidget *widget;
    GtkWidget *cardview;
    GtkWidget *logview;
    GtkWidget *readerview;
    GtkWidget *statusbar;
    GtkWidget *vbox;
    GtkWidget *entry;
    GtkWidget *tabs;
    GtkWidget *label;
    GtkAccelGroup *accel_group;

    /* main window start */

    MAIN_WINDOW = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(MAIN_WINDOW),600,400);
    gtk_container_set_border_width (GTK_CONTAINER (MAIN_WINDOW), 0);
    g_signal_connect (MAIN_WINDOW, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    /* Check if those bloody GTK icons are there */

    if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), "image-missing")==FALSE)
    {
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW(MAIN_WINDOW),
                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                 GTK_MESSAGE_ERROR,
                                 GTK_BUTTONS_CLOSE,
                                 "<b>It seems that the default GTK+ 3 icons are not correctly installed on your system.</b>\n\n"
                                 "This will cause problems in the Cardpeek user interface.\n\n"
                                 "Suggestion: Check that the <tt>gnome-icon-theme</tt> is installed on your system and that the icon cache is up-to-date.");
        
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
    }

    /* Build icon sets */

    gui_load_icons();

    /* log frame */

    widget = gui_logview_create_window();
    logview = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(logview),GTK_SHADOW_ETCHED_IN);
    gtk_container_add (GTK_CONTAINER(logview),widget);

    /* reader view frame */

    widget = gui_readerview_create_window();
    readerview = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(readerview),GTK_SHADOW_ETCHED_IN);
    gtk_container_add (GTK_CONTAINER(readerview),widget);

    /* tree view frame */

    accel_group = gtk_accel_group_new();

    widget = gui_cardview_create_window(accel_group);
    g_signal_connect(G_OBJECT(widget),"map",G_CALLBACK(internal_map_accel_group),accel_group);
    g_signal_connect(G_OBJECT(widget),"unmap",G_CALLBACK(internal_unmap_accel_group),accel_group);
    cardview = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(cardview),GTK_SHADOW_ETCHED_IN);
    gtk_container_add (GTK_CONTAINER(cardview),widget);

    /* command entry */

    entry = gui_scratchpad_create_window ();

    /* status bar */

    statusbar = gui_logview_create_status_bar();

    /* notebook */

    tabs = gtk_notebook_new ();
    MAIN_NOTEBOOK = tabs;
    g_signal_connect(tabs, "create-window", G_CALLBACK(notebook_create_extra), NULL);
    gtk_notebook_set_group_name (GTK_NOTEBOOK (tabs), "cardpeek-tabs");

    label = gtk_label_new ("card view");
    gtk_notebook_append_page (GTK_NOTEBOOK (tabs), cardview, label);
    /* gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (tabs), cardview, TRUE); */

    label = gtk_label_new ("reader");
    gtk_notebook_append_page (GTK_NOTEBOOK (tabs), readerview, label);
    /* gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (tabs), readerview, TRUE); */
    gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (tabs), readerview, TRUE);

    label = gtk_label_new ("logs");
    gtk_notebook_append_page (GTK_NOTEBOOK (tabs), logview, label);
    /* gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (tabs), logview, TRUE); */
    gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (tabs), logview, TRUE);

    /* vertical packing */

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_box_pack_start (GTK_BOX (vbox), tabs, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), statusbar, FALSE, TRUE, 0);

    /* main window finish */

    gtk_settings_set_long_property(gtk_settings_get_default(),"gtk-tooltip-timeout",1500,NULL);

    gtk_container_add (GTK_CONTAINER (MAIN_WINDOW), vbox);

    gtk_widget_show_all (MAIN_WINDOW);

#ifdef DISPLAY_SUPPORTED_MIME_TYPES
    gui_get_supported_image_mime_types();
#endif

    return 1;
}

static int gui_initialize(int *argc, char ***argv)
{
    gtk_init(argc,argv);
    return gui_create();
}

static gboolean run_command_from_cli(gpointer data)
{
    luax_run_command((const char *)data);
    return FALSE;
}

static int gui_run(const char *optional_command)
{
    if (optional_command)
        g_idle_add(run_command_from_cli,(gpointer)optional_command);

    gtk_main ();

    gui_logview_cleanup();
    gui_scratchpad_cleanup();
    gui_readerview_cleanup();
    gui_cardview_cleanup();

    MAIN_WINDOW=NULL;
    MAIN_NOTEBOOK=NULL;

    return 1;
}

static void gui_set_title(const char *title)
{
    char atitle[80];

    snprintf(atitle,80,"cardpeek: %s",title);
    atitle[79]=0;
    gtk_window_set_title(GTK_WINDOW(MAIN_WINDOW),atitle);
}

static const char *gui_driver_name(void) 
{
    return "gtk";
}


ui_driver_t UI_DRIVER_GTK = {
    gui_driver_name,
    gui_initialize,
    gui_run,
    gui_exit,
    gui_update,
    gui_select_reader,
    gui_question_l,
    gui_readline,
    gui_select_file,
    gui_set_title,
    gui_about,
    gui_inprogress_new,
    gui_inprogress_pulse,
    gui_inprogress_set_fraction,
    gui_inprogress_free,
    gui_readerview_print
};

ui_driver_t *ui_driver_for_gtk(void)
{
    return &UI_DRIVER_GTK;
}
