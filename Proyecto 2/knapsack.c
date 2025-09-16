/* Proyecto 2 IO -PR02
Estudiantes:
Emily Sanchez -
Viviana Vargas -
*/

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <math.h>
#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

//--- GTK Variables ----
//Window 1
GtkWidget   *window1;
GtkWidget   *fixed2;
GtkWidget   *title1;
GtkWidget   *label1;
//GtkWidget   *instruction;
GtkWidget   *rb_01;
GtkWidget   *rb_bounded;
GtkWidget   *rb_unbounded;
GtkWidget   *maxCapacity;
GtkWidget   *objects;
GtkWidget   *fileLoad;
GtkWidget   *loadLabel;
GtkWidget   *exitButton1;
GtkWidget   *scrollWindow;
GtkWidget   *createSolution;
GtkWidget   *fileName;
GtkWidget   *editLatexButton;
GtkWidget   *loadToGrid;
GtkWidget   *saveProblem;
GtkWidget   *instruction3;
GtkWidget   *capacityLabel;
GtkWidget   *objectsLabel;

//Builders
GtkBuilder  *builder;
GtkCssProvider *cssProvider;
//Dynamic Widgets
static GtkWidget *current_grid = NULL;
static GPtrArray *entry_costs  = NULL; 
static GPtrArray *entry_values = NULL;
static GPtrArray *entry_quantity    = NULL;

//Variables Globales
gchar *last_selected_tex = NULL;
static char *filepath = NULL;
int selected_rb = 1; // 1 -> 0/1, 2 -> Bounded, 3 -> Unbounded
static int **knapsack_table = NULL;
static int currentObjects;
static int currentCapacity;

typedef struct {
    gchar  name[8];     
    double cost;        
    double value;       
    int    quantity;
    gboolean unbounded; 
} KnapsackItem;

static gchar* name_from_index(int idx) {
    return object_name_setter(idx);
}

static gboolean has_infinity(const gchar *s) {
    if (!s) return FALSE;
    // Busca el codepoint '∞'
    return (g_utf8_strchr(s, -1, 0x221E) != NULL);
}

static gchar* normalize_decimal(const gchar *s) {
    if (!s) return g_strdup("");
    gchar *dup = g_strdup(s);
    for (char *p = dup; *p; ++p) if (*p == ',') *p = '.';
    return dup;
}

static gchar* trimdup(const gchar *s) {
    if (!s) return g_strdup("");
    const gchar *start = s;
    while (g_unichar_isspace(g_utf8_get_char(start))) start = g_utf8_next_char(start);
    const gchar *end = s + strlen(s);
    while (end > start) {
        const gchar *prev = g_utf8_find_prev_char(start, end);
        if (!prev) break;
        if (!g_unichar_isspace(g_utf8_get_char(prev))) break;
        end = prev;
    }
    return g_strndup(start, end - start);
}



//Función para que se utilice el archivo .css como proveedor de estilos.
void set_css (GtkCssProvider *cssProvider, GtkWidget *widget){
    GtkStyleContext *styleContext = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(styleContext,GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);
}

// --------- Helpers --------- 

// Setear el nombre de los objetos de A a Z
static gchar* object_name_setter(int index) {
    GString *s = g_string_new(NULL);
    int n = index;
    while (n > 0) {
        n--;
        g_string_prepend_c(s, 'A' + (n % 26));
        n /= 26;
    }
    return g_string_free(s, FALSE);
}

static void validate_entry (GtkEditable *editable, const gchar *text, gint length,gint *position, gpointer user_data) {
    gchar *filtered = g_new(gchar, length + 1);
    int j = 0;
    for (int i = 0; i < length; i++) {
        gunichar ch = g_utf8_get_char(text + i);
        if (g_unichar_isdigit(ch)) filtered[j++] = text[i];
    }
    filtered[j] = '\0';

    if (j == 0) {
        g_signal_stop_emission_by_name(editable, "insert-text");
        g_free(filtered);
        return;
    }

    g_signal_handlers_block_by_func(editable, G_CALLBACK(validate_entry), user_data);
    gtk_editable_insert_text(editable, filtered, j, position);
    g_signal_handlers_unblock_by_func(editable, G_CALLBACK(validate_entry), user_data);

    g_signal_stop_emission_by_name(editable, "insert-text");
    g_free(filtered);
}

// --------- LATEX --------- 

void compile_latex_file(const gchar *tex_file) {}

// Función para seleccionar archivo latex
void on_select_latex_file(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new("Seleccionar archivo LaTeX",
                                        GTK_WINDOW(window1),
                                        action,
                                        "Cancelar",
                                        GTK_RESPONSE_CANCEL,
                                        "Abrir",
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

    // Filtro para archivos .tex
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Archivos LaTeX (*.tex)");
    gtk_file_filter_add_pattern(filter, "*.tex");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    // Directorio por defecto a Reports carpeta de archivos
    gchar *reports_dir = g_build_filename(g_get_current_dir(), "Reports", NULL);
    if (g_file_test(reports_dir, G_FILE_TEST_IS_DIR)) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), reports_dir);
    }
    g_free(reports_dir);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        if (last_selected_tex) g_free(last_selected_tex);
        last_selected_tex = gtk_file_chooser_get_filename(chooser);
        
        // Preguntar si quiere editar o compilar
        GtkWidget *choice_dialog = gtk_dialog_new_with_buttons(
            "¿Qué deseas hacer?",
            GTK_WINDOW(window1),
            GTK_DIALOG_MODAL,
            "Editar",
            GTK_RESPONSE_YES,
            "Compilar",
            GTK_RESPONSE_NO,
            "Ambos",
            GTK_RESPONSE_APPLY,
            NULL
        );
        
        GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(choice_dialog));
        GtkWidget *label = gtk_label_new("Selecciona una opción para el archivo LaTeX:");
        gtk_container_add(GTK_CONTAINER(content_area), label);
        gtk_widget_show_all(choice_dialog);
        
        gint choice = gtk_dialog_run(GTK_DIALOG(choice_dialog));
        gtk_widget_destroy(choice_dialog);
        
        if (choice == GTK_RESPONSE_YES) {
            // Solo editar
            gchar *edit_cmd = g_strdup_printf("xdg-open \"%s\"", last_selected_tex);
            system(edit_cmd);
            g_free(edit_cmd);
        } else if (choice == GTK_RESPONSE_NO) {
            // Solo compilar
            compile_latex_file(last_selected_tex);
        } else if (choice == GTK_RESPONSE_APPLY) {
            // Editar y luego compilar
            gchar *edit_cmd = g_strdup_printf("xdg-open \"%s\"", last_selected_tex);
            system(edit_cmd);
            g_free(edit_cmd);
            
            // Preguntar después de editar si quiere compilar
            GtkWidget *compile_dialog = gtk_dialog_new_with_buttons(
                "Recompilar PDF",
                GTK_WINDOW(window1),
                GTK_DIALOG_MODAL,
                "Compilar ahora",
                GTK_RESPONSE_YES,
                "Después",
                GTK_RESPONSE_NO,
                NULL
            );
            
            GtkWidget *compile_content = gtk_dialog_get_content_area(GTK_DIALOG(compile_dialog));
            GtkWidget *compile_label = gtk_label_new("¿Deseas compilar el PDF ahora?");
            gtk_container_add(GTK_CONTAINER(compile_content), compile_label);
            gtk_widget_show_all(compile_dialog);
            
            gint compile_response = gtk_dialog_run(GTK_DIALOG(compile_dialog));
            gtk_widget_destroy(compile_dialog);
            
            if (compile_response == GTK_RESPONSE_YES) {
                compile_latex_file(last_selected_tex);
            }
        }
    }
    
    gtk_widget_destroy(dialog);
}

// --------- KNAPSACK --------- 

void build_table(int items) {
    if (!GTK_IS_SCROLLED_WINDOW(scrollWindow)) {
        return;
    }

    GList *children = gtk_container_get_children(GTK_CONTAINER(scrollWindow));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_container_remove(GTK_CONTAINER(scrollWindow), GTK_WIDGET(l->data));
    }
    g_list_free(children);

    if (entry_costs)  { 
        g_ptr_array_free(entry_costs,  TRUE); 
        entry_costs  = NULL; 
    }
    if (entry_values) { 
        g_ptr_array_free(entry_values, TRUE); 
        entry_values = NULL; 
    }
    if (entry_quantity) { 
        g_ptr_array_free(entry_quantity,TRUE); 
        entry_quantity = NULL; 
    }

    if (items < 1) {
        items = 1; 
    }

    entry_costs  = g_ptr_array_sized_new(items + 1);
    entry_values = g_ptr_array_sized_new(items + 1);
    entry_quantity    = g_ptr_array_sized_new(items + 1);

    g_ptr_array_set_size(entry_costs,  items + 1);   
    g_ptr_array_set_size(entry_values, items + 1);
    g_ptr_array_set_size(entry_quantity,items + 1);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_widget_set_vexpand(grid, TRUE);

    // Column Headers
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Object"),   0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Cost"),    1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Value"),    2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Quantity"), 3, 0, 1, 1);

    // Filas
    for (int i = 1; i <= items; i++) {
        gchar *name = object_name_setter(i);
        GtkWidget *lbl = gtk_label_new(name);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), lbl, 0, i, 1, 1);
        g_free(name);

        // Columna de costo
        GtkWidget *cost_cell = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(cost_cell), 8);
        gtk_entry_set_alignment(GTK_ENTRY(cost_cell), 1.0);
        gtk_entry_set_placeholder_text(GTK_ENTRY(cost_cell), "0.0");
        gtk_grid_attach(GTK_GRID(grid), cost_cell, 1, i, 1, 1);
        g_ptr_array_index(entry_costs, i) = cost_cell;
        g_signal_connect(cost_cell, "insert-text", G_CALLBACK(validate_entry), NULL);

        // Columna de valores
        GtkWidget *value_cell = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(value_cell), 8);
        gtk_entry_set_alignment(GTK_ENTRY(value_cell), 1.0);
        gtk_entry_set_placeholder_text(GTK_ENTRY(value_cell), "0.0");
        gtk_grid_attach(GTK_GRID(grid), value_cell, 2, i, 1, 1);
        g_ptr_array_index(entry_values, i) = value_cell;
        g_signal_connect(value_cell, "insert-text", G_CALLBACK(validate_entry), NULL);

        //Columna de cantidad
        GtkWidget *quantity_cell = gtk_entry_new();
            gtk_entry_set_width_chars(GTK_ENTRY(quantity_cell), 6);
            gtk_entry_set_alignment(GTK_ENTRY(quantity_cell), 1.0);

        // Si se seleccionó 0/1, entonces el default es 1 y no se puede editar.
        if(selected_rb == 1){
            
            gtk_entry_set_text(GTK_ENTRY(quantity_cell), "1");
            gtk_editable_set_editable(GTK_EDITABLE(quantity_cell), FALSE);
            gtk_widget_set_sensitive(quantity_cell, FALSE);
            
        } 
        // Si se seleccionó bounded, el default es 1 y se puede seleccionar otro valor.
        else if(selected_rb == 2){
            
            gtk_entry_set_text(GTK_ENTRY(quantity_cell), "1");
            gtk_editable_set_editable(GTK_EDITABLE(quantity_cell), TRUE);
            gtk_widget_set_sensitive(quantity_cell, TRUE);
            g_signal_connect(quantity_cell, "insert-text", G_CALLBACK(validate_entry), NULL);
            
        } 
        // Se se seleccionó unbounded, el default es el infinito y no se puede editar.
        else if(selected_rb == 3){
            
            gtk_entry_set_text(GTK_ENTRY(quantity_cell), "∞");
            gtk_editable_set_editable(GTK_EDITABLE(quantity_cell), FALSE);
            gtk_widget_set_sensitive(quantity_cell, FALSE);
            
        }

        gtk_grid_attach(GTK_GRID(grid), quantity_cell, 3, i, 1, 1);
        g_ptr_array_index(entry_quantity, i) = quantity_cell;
        
    }

    gtk_container_add(GTK_CONTAINER(scrollWindow), grid);
    gtk_widget_show_all(grid);
    current_grid = grid;
}

GArray* read_knapsack_items(int n_items) {
    if (!current_knapsack_grid || !entry_costs || !entry_values || !entry_qty) return NULL;
    if (n_items <= 0) return NULL;

    GArray *items = g_array_new(FALSE, FALSE, sizeof(KnapsackItem));

    for (int i = 1; i <= n_items; i++) {
        KnapsackItem obj;
        memset(&obj, 0, sizeof(obj));
        obj.unbounded = FALSE;
        obj.quantity  = 0;

        // Nombre
        gchar *nm = name_from_index(i);
        g_strlcpy(obj.name, nm ? nm : "?", sizeof(obj.name));
        g_free(nm);

        // Costo
        const gchar *tc_raw = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(entry_costs, i)));
        gchar *tc_norm = normalize_decimal(tc_raw);
        char *endp = NULL;
        obj.cost = g_ascii_strtod(tc_norm, &endp);
        if (obj.cost < 0) obj.cost = 0;
        g_free(tc_norm);

        // Valor
        const gchar *tv_raw = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(entry_values, i)));
        gchar *tv_norm = normalize_decimal(tv_raw);
        endp = NULL;
        obj.value = g_ascii_strtod(tv_norm, &endp);
        if (obj.value < 0) obj.value = 0;
        g_free(tv_norm);

        // Cantidad
        const gchar *tq_raw = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(entry_qty, i)));
        gchar *tq = trimdup(tq_raw);

        if (has_infinity(tq)) {
            obj.unbounded = TRUE;
            obj.quantity  = -1; 
        } else {
            long q = strtol(tq, &endp, 10);
            if (q < 0) q = 0;
            obj.quantity = (int)q;
        }
        g_free(tq);

        g_array_append_val(items, obj);
    }

    return items;
}

// --------- BOTONES --------- 

G_MODULE_EXPORT void on_objects_value_changed(GtkSpinButton *objects, gpointer user_data) {
    int n = gtk_spin_button_get_value_as_int(objects);
    build_table(n);
}

G_MODULE_EXPORT void on_rb_01_toggled (GtkRadioButton *rb_01, gpointer user_data) {
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_01))) {
        return;
    }
    selected_rb = 1;
    int n = gtk_spin_button_get_value_as_int(objects);
    build_table(n);
}

G_MODULE_EXPORT void on_rb_bounded_toggled (GtkRadioButton *rb_bounded, gpointer user_data) {
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_bounded))) {
        return;
    }
    selected_rb = 2;
    int n = gtk_spin_button_get_value_as_int(objects);
    build_table(n);
}

G_MODULE_EXPORT void on_rb_unbounded_toggled (GtkRadioButton *rb_unbounded, gpointer user_data) {
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_unbounded))) {
        return;
    }
    selected_rb = 3;
    int n = gtk_spin_button_get_value_as_int(objects);
    build_table(n);
}

G_MODULE_EXPORT void on_fileLoad_file_set (GtkWidget *loadProblem, gpointer data){
	gtk_widget_set_sensitive(loadToGrid, TRUE);
    gtk_widget_set_sensitive(saveProblem, TRUE);
}


//Función de acción para el botón de 'Exit' que cierra todo el programa.
void on_exitButton_clicked (GtkButton *exitButton1, gpointer data){
    gtk_main_quit();
}

// Función para el botón de editar latex
void on_editLatex_clicked(GtkWidget *editLatex, gpointer data) {
    // Primero verificar si ya hay un archivo seleccionado 
    if (last_selected_tex && g_file_test(last_selected_tex, G_FILE_TEST_EXISTS)) {
        GtkWidget *dialog = gtk_dialog_new_with_buttons(
            "Archivo LaTeX encontrado",
            GTK_WINDOW(window1),
            GTK_DIALOG_MODAL,
            "Usar archivo anterior",
            GTK_RESPONSE_YES,
            "Seleccionar nuevo",
            GTK_RESPONSE_NO,
            NULL
        );
        
        gchar *message = g_strdup_printf("¿Deseas usar el archivo anterior?\n%s", last_selected_tex);
        GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        GtkWidget *label = gtk_label_new(message);
        gtk_container_add(GTK_CONTAINER(content), label);
        gtk_widget_show_all(dialog);
        
        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_free(message);
        
        if (response == GTK_RESPONSE_YES) {
            // Usar archivo anterior
            gchar *edit_cmd = g_strdup_printf("xdg-open \"%s\"", last_selected_tex);
            system(edit_cmd);
            g_free(edit_cmd);
            return;
        }
    }
    
    // Si no hay archivo anterior o el usuario quiere seleccionar nuevo
    on_select_latex_file(editLatex, data);
}

void on_createSolution_clicked (GtkWidget *btn, gpointer data) {
    int n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(objects));
    GArray *items = read_knapsack_items(n);
    if (!items) return;

    // Solo para verificar en consola
    for (guint i = 0; i < items->len; i++) {
        KnapsackItem *it = &g_array_index(items, KnapsackItem, i);
        g_print("Item %s  cost=%.3f  value=%.3f  qty=%d  %s\n",
                it->name, it->cost, it->value, it->quantity,
                it->unbounded ? "(unbounded)" : "");
    }

    // Aquí sería generar la solución

    g_array_free(items, TRUE);
}

void on_loadToGrid_clicked (GtkWidget *loadToGrid, gpointer data){}

void on_saveProblem_clicked (GtkWidget *saveProblem, gpointer data){}



//Main
int main (int argc, char *argv[]){
    gtk_init(&argc, &argv);
    
    builder =  gtk_builder_new_from_file ("Knapsack.glade");
    
    window1 = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
    
    g_signal_connect(window1, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    gtk_builder_connect_signals(builder, NULL);
    
    fixed2 = GTK_WIDGET(gtk_builder_get_object(builder, "fixed1"));
    title1 = GTK_WIDGET(gtk_builder_get_object(builder, "title1"));
    //description = GTK_WIDGET(gtk_builder_get_object(builder, "description"));
    label1 = GTK_WIDGET(gtk_builder_get_object(builder, "label1"));
    maxCapacity = GTK_WIDGET(gtk_builder_get_object(builder, "maxCapacity"));
    objects = GTK_WIDGET(gtk_builder_get_object(builder, "objects"));
    rb_01 = GTK_WIDGET(gtk_builder_get_object(builder, "rb_01"));
    rb_bounded = GTK_WIDGET(gtk_builder_get_object(builder, "rb_bounded"));
    rb_unbounded = GTK_WIDGET(gtk_builder_get_object(builder, "rb_unbounded"));
    exitButton1 = GTK_WIDGET(gtk_builder_get_object(builder, "exitButton1"));
    fileLoad = GTK_WIDGET(gtk_builder_get_object(builder, "fileLoad"));
    loadLabel = GTK_WIDGET(gtk_builder_get_object(builder, "loadLabel"));
    scrollWindow = GTK_WIDGET(gtk_builder_get_object(builder, "scrollWindow"));
    createSolution = GTK_WIDGET(gtk_builder_get_object(builder, "createSolution"));
    fileName = GTK_WIDGET(gtk_builder_get_object(builder, "fileName"));
    loadToGrid = GTK_WIDGET(gtk_builder_get_object(builder, "loadToGrid"));
    saveProblem = GTK_WIDGET(gtk_builder_get_object(builder, "saveProblem"));
    editLatexButton = GTK_WIDGET(gtk_builder_get_object(builder, "editLatexButton"));
    capacityLabel = GTK_WIDGET(gtk_builder_get_object(builder, "capacityLabel"));
    objectsLabel = GTK_WIDGET(gtk_builder_get_object(builder, "objectsLabel"));


    cssProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(cssProvider, "theme.css", NULL);

    set_css(cssProvider, window1);
    set_css(cssProvider, fileLoad);
    set_css(cssProvider, exitButton1);
    set_css(cssProvider, scrollWindow);
    set_css(cssProvider, createSolution);
    set_css(cssProvider, editLatexButton);

    g_signal_connect(editLatexButton, "clicked", G_CALLBACK(on_editLatex_clicked), NULL);
    g_signal_connect(exitButton1, "clicked", G_CALLBACK(on_exitButton_clicked), NULL);
    g_signal_connect(createSolution, "clicked", G_CALLBACK(on_createSolution_clicked), NULL);
    g_signal_connect(loadToGrid, "clicked", G_CALLBACK(on_loadToGrid_clicked), NULL);
    g_signal_connect(saveProblem, "clicked", G_CALLBACK(on_saveProblem_clicked), NULL);
    g_signal_connect(objects, "value-changed",G_CALLBACK(on_objects_value_changed), NULL);
    gtk_widget_set_sensitive(loadToGrid, FALSE);
    gtk_widget_set_sensitive(saveProblem, FALSE);
    
    build_table( gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(objects)) );
    gtk_widget_show(window1);
    
    gtk_main();

    return EXIT_SUCCESS;
}
