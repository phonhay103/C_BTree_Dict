#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "lib/bt-5.0.0/inc/btree.h"
#include "lib/soundex.h"
#include "lib/trie.h"
#include <gtk/gtk.h>
#include <gdk/gdk.h>
// #include <pango/pango.h>
// #include <time.h>

#define dsize 20000*sizeof(char)

gboolean search(GtkWidget* widget, GdkEvent *event, gpointer data);
void createTrie();
char* Autocorrect(gchar *st);
int Autocomplete2(const gchar *query, gchar output[MAX_SUGGETS][MAX_LENGTH]);
void set_text_view_text(GtkWidget* view, gchar* text);
void show_message(GtkWindow* parent, GtkMessageType type, gchar* data1, gchar* data2);
void word_add(GtkWidget* widget, gpointer data);
void input_from_file(char* file_name);
void word_edit(GtkWidget* widget, gchar* data);
void about_dialog(GtkWidget* widget, gpointer data);
void word_delete(GtkWidget* widget, gpointer data);
void delete_by_button(GtkWidget* widget, gpointer data);
void show_all(GtkWidget* widget, gpointer data);
void show(GtkLabel* lable, gpointer data);
void aaaa(GtkWidget* widget, gpointer data);

GtkBuilder* builder;
TrieNode *_root;
BTA* Dict;
GtkWidget *text_view;
GtkLabel* label;
GtkListStore* list;
GtkEntryCompletion* completion;
GtkTreeIter iter;
int num_suggest_words;
gchar output[MAX_SUGGETS][MAX_LENGTH];
GtkWidget *word_view, *mean_view;
FILE *log_file;

static void activate (GtkApplication* app, gpointer user_data)
{
    builder = gtk_builder_new_from_file ("GUI/builder.glade");

    GtkWidget* window = GTK_WIDGET(gtk_builder_get_object (builder, "window"));
    gtk_window_set_title(GTK_WINDOW(window), "Dictionary");
    gtk_application_add_window (app, GTK_WINDOW(window));
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_widget_destroy), NULL);

    GtkWidget* search_entry = GTK_WIDGET(gtk_builder_get_object (builder, "search_entry"));
    g_signal_connect(search_entry, "key-press-event", G_CALLBACK(search), NULL);

    GtkWidget* search_button = GTK_WIDGET(gtk_builder_get_object (builder, "search_button"));
    g_signal_connect_swapped(search_button, "clicked", G_CALLBACK(search), search_entry);

    text_view = GTK_WIDGET(gtk_builder_get_object (builder, "text_view"));
    label = GTK_LABEL(gtk_builder_get_object (builder, "text_view_label"));
    list = gtk_list_store_new (1, G_TYPE_STRING);

    completion = gtk_entry_completion_new();
    gtk_entry_completion_set_model (completion, GTK_TREE_MODEL(list));
    gtk_entry_set_completion (GTK_ENTRY(search_entry), completion);
    gtk_entry_completion_set_text_column(completion, 0);
    gtk_entry_completion_set_inline_completion (completion, true);
    gtk_entry_completion_set_inline_selection (completion, true);

    GtkMenuItem* quit = GTK_MENU_ITEM(gtk_builder_get_object(builder, "quit"));
    g_signal_connect_swapped (quit, "activate", G_CALLBACK(gtk_widget_destroy), window);
    GtkWidget* exit_button = GTK_WIDGET(gtk_builder_get_object(builder, "exit_button"));
    g_signal_connect_swapped (exit_button, "clicked", G_CALLBACK(gtk_widget_destroy), window);

    GtkMenuItem* about = GTK_MENU_ITEM(gtk_builder_get_object(builder, "about"));
    g_signal_connect(about, "activate", G_CALLBACK(about_dialog), NULL);

    GtkMenuItem* add_item = GTK_MENU_ITEM(gtk_builder_get_object(builder, "add_item"));
    g_signal_connect_swapped(add_item, "activate", G_CALLBACK(word_add), window);
    GtkWidget* add_button = GTK_WIDGET(gtk_builder_get_object(builder, "add_button"));
    g_signal_connect_swapped(add_button, "clicked", G_CALLBACK(word_add), window);

    GtkMenuItem* delete_item = GTK_MENU_ITEM(gtk_builder_get_object(builder, "delete_item"));
    g_signal_connect(delete_item, "activate", G_CALLBACK(word_delete), NULL);
    GtkWidget* delete_button = GTK_WIDGET(gtk_builder_get_object(builder, "delete_button"));
    g_signal_connect_swapped(delete_button, "clicked", G_CALLBACK(delete_by_button), window);

    GtkMenuItem* edit_item = GTK_MENU_ITEM(gtk_builder_get_object(builder, "edit_item"));
    g_signal_connect(edit_item, "activate", G_CALLBACK(word_edit), NULL);
    GtkWidget* edit_button = GTK_WIDGET(gtk_builder_get_object(builder, "edit_button"));
    g_signal_connect(edit_button, "clicked", G_CALLBACK(word_edit), NULL);

    GtkWidget* show_item = GTK_WIDGET(gtk_builder_get_object(builder, "show_all"));
    g_signal_connect_swapped(show_item, "activate", G_CALLBACK(show_all), window);

    GtkWidget* aaa = GTK_WIDGET(gtk_builder_get_object(builder, "aaa")); // ?
    g_signal_connect_swapped(aaa, "activate", G_CALLBACK(aaaa), window); // ?
        
    gtk_widget_show_all (GTK_WIDGET(window));
}

int main(int argc, char* argv[])
{
    // Init
    btinit();
    Dict = btopn("data/Dict.txt", 0, FALSE);
    if (!Dict)
    {
        Dict = btcrt("data/Dict.txt", 0, FALSE);
        printf("Dang tao tu dien...\n");
        input_from_file("data/AnhViet.dict");
        if (!Dict)
            return -1;
    }

    printf("Nap du lieu...\n");
    createTrie();
    if(_root == NULL)
    {
        btcls(Dict);
        return -1;
    }

    log_file = fopen("log/logs.txt", "a");

    GtkApplication* app = gtk_application_new ("dict.TL2D", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
    int status = g_application_run (G_APPLICATION (app), argc, argv);

    // End
    printf("Exit\n");
    g_object_unref (app);
    Trie_delete(&_root);
    btcls(Dict);
    fclose(log_file);
    return status;
}

// input B-tree keys into Trie
void createTrie()
{
    if (btpos(Dict, ZSTART))
    {
        _root = NULL;
        return;
    }
    long c = 0;
    _root = createNode();
	char key[MAX_LENGTH], val[dsize];
	int rsize;
	while (!btseln(Dict, key, val, dsize, &rsize)){
        Trie_insert(_root, key);c++;}
    printf("C = %ld\n", c);
}

char* Autocorrect(gchar *st)
{
    if (btpos(Dict, ZSTART))
		return NULL;

	char key[MAX_LENGTH], val[dsize];
	int rsize;
    char code[] = "     ";
    char code_t[] = "     ";
    soundex(st, code);
	while (!btseln(Dict, key, val, dsize, &rsize))
	{
        soundex(key, code_t);
        if (strcmp(code, code_t) == 0)
        {
            strcpy(st, key);
            return strdup(val);
        }
    }
    return NULL;
}

void suggestions2(TrieNode* root, gchar* currPrefix, int *n, gchar output[MAX_SUGGETS][MAX_LENGTH])
{
    if (*n == MAX_SUGGETS) return;
    if (root->isEndOfWord)
        strcpy(output[(*n)++], currPrefix);
    if (isEmpty(root)) return;

    for (int i = 0; i < ALPHABET_SIZE; i++)
    {
        if (root->children[i])
        {
            char ch = INDEX_TO_CHAR(i);
            strncat(currPrefix, &ch, 1);
            suggestions2(root->children[i], currPrefix, n, output);
            currPrefix[strlen(currPrefix)-1] = '\0';
        }
    }
}

int Autocomplete2(const gchar *query, gchar output[MAX_SUGGETS][MAX_LENGTH])
{
    TrieNode* pCrawl = _root;

    int length = strlen(query);
    for (int level = 0; level < length; level++)
    {
        int index = CHAR_TO_INDEX(query[level]);
        if (!pCrawl->children[index]) return 0;
        pCrawl = pCrawl->children[index];
    }
    
    bool isWord = (pCrawl->isEndOfWord == true);
    bool isLast = isEmpty(pCrawl);

    if (isWord && isLast)
    {
        strcpy(output[0], query);
        return 1;
    }

    if (!isLast)
    {
        int n = 0;
        gchar prefix[50];
        strcpy(prefix, query);
        suggestions2(pCrawl, prefix, &n, output);
        return n;
    }
    return 0;
}

void set_text_view_text(GtkWidget* view, gchar* text)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	if (buffer == NULL)
		buffer = gtk_text_buffer_new(NULL);
	gtk_text_buffer_set_text(buffer, text, -1);
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(view), buffer);
}

void show_message(GtkWindow* parent, GtkMessageType type, gchar* data1, gchar* data2)
{
    GtkWidget* dialog = gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT, type, GTK_BUTTONS_OK, "%s", data1);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", data2);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void add(GtkWidget* widget, gpointer data) // Word add event
{
    gchar word[MAX_LENGTH], mean[dsize];
    gchar _mean[dsize]; int rsize;
    strcpy(word, gtk_entry_get_text(GTK_ENTRY(word_view)));

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mean_view));
    GtkTextIter iter, start, end;
    gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);
    gtk_text_buffer_insert(buffer, &iter, "", -1);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    strcpy(mean, gtk_text_buffer_get_text(buffer, &start, &end, false));

    if (strcmp(word, "") == 0)
    {
        show_message (GTK_WINDOW(widget), GTK_MESSAGE_WARNING, "Type Error", "Tu khong duoc de trong");
        return;
    }
    else
    {
        for (int i = 0; i < strlen(word); ++i)
            if (word[i] < 'a' || word[i] > 'z')
            {
                show_message (GTK_WINDOW(widget), GTK_MESSAGE_WARNING, "Type Error", "Chi nhap ky tu a-z");
                return;
            }
    }

    if (!btsel(Dict, word, _mean, dsize, &rsize))
        show_message(GTK_WINDOW(widget), GTK_MESSAGE_WARNING, "Exist", "Tu vua nhap da co trong tu dien");
    else 
    {
        int status = btins(Dict, word, mean, dsize);
        if (status == 52)
            show_message(GTK_WINDOW(widget), GTK_MESSAGE_ERROR, "ERROR", "Tu dien da day");
        else if (status == 0)
        {
            show_message(GTK_WINDOW(widget), GTK_MESSAGE_INFO, "Success", "Them tu thanh cong");
            Trie_insert(_root, word);

        /****************** Log ************/
        fprintf(log_file, "ADD_%s\n", word);
        /************************************/
        }
        else
            show_message(GTK_WINDOW(widget), GTK_MESSAGE_ERROR, "ERROR", "Something wrong");
    }
}

void word_add(GtkWidget* widget, gpointer data)
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Them tu");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 450);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 12);
    gtk_window_set_resizable(GTK_WINDOW(window), false);
    gtk_window_set_keep_above(GTK_WINDOW(window), true);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect_swapped(widget, "destroy", G_CALLBACK(gtk_window_close), window);
    
    GtkWidget* fixed = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(window), fixed);

    GtkWidget* button = gtk_button_new_with_label("Add");
    gtk_widget_set_size_request (button, 100, 40);
    gtk_fixed_put(GTK_FIXED(fixed), button, 500, 30);
    g_signal_connect_swapped(button, "clicked", G_CALLBACK(add), window);
    
    word_view = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(word_view), "Input");
    gtk_entry_set_max_length(GTK_ENTRY(word_view), 30);
    gtk_widget_set_size_request (word_view, 450, 40);
    gtk_fixed_put(GTK_FIXED(fixed), word_view, 35, 30);

    GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, 450, 350);
    gtk_fixed_put(GTK_FIXED(fixed), scrolled_window, 35, 90);

    mean_view = gtk_text_view_new();
    set_text_view_text(mean_view, "\0");
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(mean_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scrolled_window), mean_view);

    gtk_widget_show_all(window);
    gtk_main();
}

void save(GtkWidget* widget, gchar* data) // Word save event
{
    gchar word[MAX_LENGTH], mean[dsize];
    strcpy(word, gtk_label_get_text(GTK_LABEL(word_view)));

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mean_view));
    GtkTextIter iter, start, end;
    gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);
    gtk_text_buffer_insert(buffer, &iter, "", -1);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    strcpy(mean, gtk_text_buffer_get_text(buffer, &start, &end, false));

    if (!btupd(Dict, word, mean, dsize))
    {
        show_message(GTK_WINDOW(widget), GTK_MESSAGE_INFO, "Success", "Thay doi thanh cong");
        set_text_view_text(text_view, mean);

        /**************** Log **************/
        fprintf(log_file, "EDIT_%s\n", word);
        /***********************************/
    }
    else
        show_message(GTK_WINDOW(widget), GTK_MESSAGE_ERROR, "ERROR", "Something wrong");
}

void word_edit(GtkWidget* widget, gchar* data)
{
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Cap nhat tu");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 450);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 12);
    gtk_window_set_resizable(GTK_WINDOW(window), false);
    gtk_window_set_keep_above(GTK_WINDOW(window), true);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect_swapped(widget, "destroy", G_CALLBACK(gtk_window_close), window);
    
    GtkWidget* fixed = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(window), fixed);

    GtkWidget* button = gtk_button_new_with_label("Save");
    gtk_widget_set_size_request (button, 100, 40);
    gtk_fixed_put(GTK_FIXED(fixed), button, 500, 20);
    g_signal_connect_swapped(button, "clicked", G_CALLBACK(save), window);
    
    gchar word[MAX_LENGTH], mean[dsize];
    strcpy(word, gtk_label_get_text(label));
    if (strcmp(word, "Hello World") == 0)
        return;
    
    word_view = gtk_label_new(word);
    gtk_widget_set_size_request (word_view, 450, 40);
    gtk_label_set_attributes(GTK_LABEL(word_view), gtk_label_get_attributes(label));
    gtk_fixed_put(GTK_FIXED(fixed), word_view, 35, 20);

    GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, 450, 350);
    gtk_fixed_put(GTK_FIXED(fixed), scrolled_window, 35, 85);

    mean_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(mean_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scrolled_window), mean_view);

    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter iter, start, end;
    gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);
    gtk_text_buffer_insert(buffer, &iter, "", -1);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    strcpy(mean, gtk_text_buffer_get_text(buffer, &start, &end, false));
    set_text_view_text(mean_view, mean);

    gtk_widget_show_all(window);
    gtk_main();
}

void about_dialog(GtkWidget* widget, gpointer data)
{
    GtkWidget* dialog = gtk_about_dialog_new ();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "Dictionary");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "Version 2.3.0");
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), "Copyright © 2020 The TL2D Team");
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dialog), GTK_LICENSE_GPL_3_0);
    gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(dialog), NULL);
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), "Doan Anh Tuan\nLe Ngoc Long\nMai Van Linh\nBui Minh Duc");

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void input_from_file(char* file_name) // Create B-Tree Dict from txt file
{
    int c = 20000;
    FILE* f = fopen(file_name, "r");
    char st[MAX_LENGTH], word[dsize], mean[dsize] = "";
    fgets(word, MAX_LENGTH, f);
    word[strlen(word)-1] = '\0'; // word[strlen(word)-1] = '\n' => '\0'
    for (int i = 0; i < strlen(word)-1; ++i)
        word[i] = word[i+4]; // => infoinfoinfo || loi gi day
    while (fgets(st, MAX_LENGTH, f))
    {
        if (st[0] == '@')
        {
            if (--c == 0)
                break;
            btins(Dict, word, mean, dsize);
            strcpy(mean, "");
            strcpy(word, st);
            word[strlen(word)-1] = '\0'; // bo \n o cuoi
            for (int i = 0; i < strlen(word)-1; ++i)
                word[i] = word[i+1]; // bo @
        }
        else
            strncat(mean, st, strlen(st)); // noi cac dong chua nghia cua tu vao trong 'mean'
    }
    fclose(f);
}

gboolean search(GtkWidget* widget, GdkEvent *event, gpointer data)
{
    GdkEventKey *keyEvent = (GdkEventKey *)event;
    gchar prefix[MAX_LENGTH];
    strcpy(prefix, gtk_entry_get_text(GTK_ENTRY(widget)));

    for (int i = 0; i < strlen(prefix); ++i)
        if ((prefix[i] < 'a') || (prefix[i] > 'z'))
            return false;
    
    num_suggest_words = Autocomplete2(prefix, output);
    gtk_list_store_clear (list);

    for (int i = 0; i < num_suggest_words; ++i)
    {
        gtk_list_store_append (list, &iter);
        gtk_list_store_set (list, &iter, 0, output[i], -1);
    }

    if (keyEvent->keyval == GDK_KEY_BackSpace)
    {
        num_suggest_words = Autocomplete2(prefix, output);
        gtk_list_store_clear (list);
        for (int i = 0; i < num_suggest_words; ++i)
        {
            gtk_list_store_append (list, &iter);
            gtk_list_store_set (list, &iter, 0, output[i], -1);
        }
    }
    else
        if (keyEvent->keyval == GDK_KEY_Tab)
        {
            if (num_suggest_words != 0)
            {
                gtk_entry_set_text(GTK_ENTRY(widget), output[0]);
                // gtk_editable_set_position(GTK_EDITABLE(widget), strlen(output[0]));
            }
            // else
                // gtk_editable_set_position(GTK_EDITABLE(widget), strlen(prefix));
        }
    else
        if (keyEvent->keyval == GDK_KEY_Return || event->type < -1 || event->type > 9999) // can nhac doan nay
        {
            if (strcmp(prefix, "") == 0)
                return false;

            char data[dsize];
            int rsize;
            if (!btsel(Dict, prefix, data, dsize, &rsize))
            {
                gtk_label_set_text(label, prefix);
                set_text_view_text(text_view, data);

                    /****************** Log *****************/
                    fprintf(log_file, "SEARCH_%s\n", prefix);
                    /****************************************/
            }
            else
            {
                char* str = Autocorrect(prefix);
                if (!str)
                {
                    gtk_label_set_text(label, "Hello World");
                    set_text_view_text(text_view, "\0");
                    show_message(NULL, GTK_MESSAGE_WARNING, "Not Exist", "Khong tim thay tu");
                }
                else
                {
                    gtk_label_set_text(label, prefix);
                    set_text_view_text(text_view, str);

                    /****************** Log *****************/
                    fprintf(log_file, "SEARCH_%s\n", prefix);
                    /****************************************/
                }
            }
        }
    return false;
}

void delete_by_button(GtkWidget* widget, gpointer data) // Delete word when click delete button
{
    gchar word[MAX_LENGTH];
    strcpy(word, gtk_label_get_text(label));
    if (strcmp(word, "Hello World") == 0)
        return;

    if (btdel(Dict, word) == 0)
    {
        show_message(GTK_WINDOW(widget), GTK_MESSAGE_INFO, "Succces", "Xoa tu thanh cong");
        gtk_label_set_text(label, "Hello World");
        set_text_view_text(text_view, "");
        Trie_remove(_root, word, 0);

        /****************** Log ****************/
        fprintf(log_file, "DELETE_%s\n", word);
        /***************************************/
    }
    else
        show_message(GTK_WINDOW(widget), GTK_MESSAGE_ERROR, "ERROR", "Something wrong");
}

void delete(GtkWidget* widget, gpointer data) // Delete event
{
    gchar word[MAX_LENGTH];
    gchar mean[dsize]; int rsize;
    strcpy(word, gtk_entry_get_text(GTK_ENTRY(word_view)));

    if (strcmp(word, "") == 0)
    {
        show_message (GTK_WINDOW(widget), GTK_MESSAGE_WARNING, "Type Error", "Tu khong duoc de trong");
        return;
    }
    else
    {
        for (int i = 0; i < strlen(word); ++i)
            if (word[i] < 'a' || word[i] > 'z')
            {
                show_message (GTK_WINDOW(widget), GTK_MESSAGE_WARNING, "Type Error", "Chi nhap ky tu a-z");
                return;
            }
    }

    if (btsel(Dict, word, mean, dsize, &rsize) == 47)
        show_message(GTK_WINDOW(widget), GTK_MESSAGE_WARNING, "Not Exist", "Tu vua nhap khong co trong tu dien");
    else
    {
        int status = btdel(Dict, word);
        if (status == 0)
        {
            show_message(GTK_WINDOW(widget), GTK_MESSAGE_INFO, "Success", "Xoa tu thanh cong");
            Trie_remove(_root, word, 0);

            /****************** Log ****************/
            fprintf(log_file, "DELETE_%s\n", word);
            /***************************************/
        }
        else
           show_message(GTK_WINDOW(widget), GTK_MESSAGE_ERROR, "ERROR", "Something wrong");
    }   
}

void word_delete(GtkWidget* widget, gpointer data)
{
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Xoa tu");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 300);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 12);
    gtk_window_set_resizable(GTK_WINDOW(window), false);
    gtk_window_set_keep_above(GTK_WINDOW(window), true);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect_swapped(widget, "destroy", G_CALLBACK(gtk_window_close), window);
    
    GtkWidget* fixed = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(window), fixed);

    GtkWidget* button = gtk_button_new_with_label("Delete");
    gtk_widget_set_size_request (button, 100, 40);
    gtk_fixed_put(GTK_FIXED(fixed), button, 400, 80);
    g_signal_connect_swapped(button, "clicked", G_CALLBACK(delete), window);
    
    word_view = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(word_view), "Input");
    gtk_entry_set_max_length(GTK_ENTRY(word_view), 30);
    gtk_widget_set_size_request (word_view, 300, 40);
    gtk_fixed_put(GTK_FIXED(fixed), word_view, 50, 80);

    gtk_widget_show_all(window);
    gtk_main();    
}

void show(GtkLabel* lable, gpointer data)
    gchar val[dsize];
	int rsize;

    if (!btsel(Dict, data, val, dsize, &rsize))
    {
        gtk_label_set_text(GTK_LABEL(word_view), data);
        set_text_view_text(mean_view, val);
    }
}

void show_all(GtkWidget* widget, gpointer data) // Show all words of dict
{
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Full Dictionary");
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 720);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 12);
    gtk_window_set_resizable(GTK_WINDOW(window), false);
    gtk_window_set_keep_above(GTK_WINDOW(window), true);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect_swapped(widget, "destroy", G_CALLBACK(gtk_window_close), window);

    GtkWidget* fixed = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(window), fixed);

    GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, 625, 550);
    gtk_fixed_put(GTK_FIXED(fixed), scrolled_window, 350, 100);

    mean_view = gtk_text_view_new();
    set_text_view_text(mean_view, "\0");
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(mean_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(mean_view), false);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(mean_view), false);
    gtk_container_add(GTK_CONTAINER(scrolled_window), mean_view);

    word_view = gtk_label_new("Hello World");
    gtk_widget_set_size_request (word_view, 625, 40);
    gtk_label_set_attributes(GTK_LABEL(word_view), gtk_label_get_attributes(label));
    gtk_fixed_put(GTK_FIXED(fixed), word_view, 350, 40);

    GtkWidget* scrolled_window2 = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window2), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window2, 150, 650);
    gtk_fixed_put(GTK_FIXED(fixed), scrolled_window2, 10, 10);

    GtkWidget* viewport = gtk_viewport_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window2), viewport);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(viewport), box);

    GtkWidget* b;
    btpos(Dict, ZSTART);
    char key[MAX_LENGTH], val[dsize];
	int rsize;
    char *markup;
    while (!btseln(Dict, key, val, dsize, &rsize))
	{
        b = gtk_label_new(NULL);
        gtk_label_set_track_visited_links(GTK_LABEL(b), false);
        markup = g_markup_printf_escaped ("<a href=\"%s\"><b>%s</b></a>", key, key);
        gtk_label_set_markup (GTK_LABEL (b), markup);
        g_free (markup);
        gtk_widget_set_halign(b, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(box), b, true, false, 0);
        g_signal_connect(b, "activate-link", G_CALLBACK(show), NULL);
    }

    gtk_widget_show_all(window);
    gtk_main();
}

void aaaa2(GtkWidget* widget, gpointer data) // ?
{
    char st[100], age[100];
    int flag = true;
    strcpy(age, gtk_entry_get_text(GTK_ENTRY(word_view)));
    if (strlen(age) == 1)
        flag = false;
    else
    {
        for (int i = 0; i < strlen(age); ++i)
        if (age[i] < '0' || age[i] > '9')
        {
            flag = false;
            break;
        }        
    }
    
    if (flag)
        sprintf(st, "Tuoi cua ban la: %s", age);
    else
        strcpy(st, "Tuoi cua ban la: ?????");
    show_message(GTK_WINDOW(widget), GTK_MESSAGE_INFO, "Hello", st);
}

void aaaa(GtkWidget* widget, gpointer data) // ?
{
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "AAA");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 450);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 12);
    gtk_window_set_resizable(GTK_WINDOW(window), false);
    gtk_window_set_keep_above(GTK_WINDOW(window), true);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect_swapped(widget, "destroy", G_CALLBACK(gtk_window_close), window);
    
    GtkWidget* fixed = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(window), fixed);

    GtkWidget* button = gtk_button_new_with_label("Check");
    gtk_widget_set_size_request (button, 100, 40);
    gtk_fixed_put(GTK_FIXED(fixed), button, 500, 50);
    g_signal_connect_swapped(button, "clicked", G_CALLBACK(aaaa2), window);
    
    GtkWidget* lb = gtk_label_new("Nhap tuoi cua ban: ");
    gtk_fixed_put(GTK_FIXED(fixed), lb, 35, 10);

    word_view = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(word_view), 3);
    gtk_widget_set_size_request (word_view, 450, 40);
    gtk_fixed_put(GTK_FIXED(fixed), word_view, 35, 50);

    gtk_widget_show_all(window);
    gtk_main();
}