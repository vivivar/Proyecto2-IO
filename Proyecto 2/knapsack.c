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
#include <glib/gstdio.h>
#include <limits.h>

//--- GTK Variables ----
//Window 1
GtkWidget   *window1;
GtkWidget   *fixed2;
GtkWidget   *title1;
GtkWidget   *label1;
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
#define MAX_CAP 20
#define MAX_OBJ 10

typedef struct {
    gchar  name[8];     
    double cost;        
    double value;       
    int    quantity;
    gboolean unbounded; 
} KnapsackItem;

typedef struct {
    int value;
    int counts[MAX_OBJ];
} BoundedCell;

void set_css(GtkCssProvider *cssProvider, GtkWidget *widget);
gchar* object_name_setter(int index);
void validate_entry(GtkEditable *editable, const gchar *text, gint length, gint *position, gpointer user_data);
int knapsack_01(int n, int W, KnapsackItem objs[]);
int knapsack_bounded(int n, int W, KnapsackItem objs[]);
int knapsack_unbounded(int n, int W, KnapsackItem objs[]);
void compile_latex_file(const gchar *tex_file);
void on_select_latex_file(GtkWidget *widget, gpointer data);
void build_table(int items);
GArray* read_knapsack_items(int n_items);
void generate_latex_report(int capacity, GArray *items, int max_value, int problem_type, int **dp, int n);
void free_knapsack_table(void);

//Función para que se utilice el archivo .css como proveedor de estilos.
void set_css(GtkCssProvider *cssProvider, GtkWidget *widget) {
    GtkStyleContext *styleContext = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(styleContext, GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);
}

// --------- Helpers --------- 

// Setear el nombre de los objetos de A a Z
gchar* object_name_setter(int index) {
    static char name[8];
    int n = index;
    int i = 0;
    
    if (n <= 0) {
        name[0] = 'A';
        name[1] = '\0';
        return name;
    }
    
    while (n > 0 && i < 7) {
        name[i++] = 'A' + ((n - 1) % 26);
        n = (n - 1) / 26;
    }
    name[i] = '\0';
    
    // Invertir la cadena
    for (int j = 0; j < i / 2; j++) {
        char temp = name[j];
        name[j] = name[i - j - 1];
        name[i - j - 1] = temp;
    }
    
    return name;
}

void validate_entry(GtkEditable *editable, const gchar *text, gint length, gint *position, gpointer user_data) {
    gchar *filtered = g_new(gchar, length + 1);
    int j = 0;
    for (int i = 0; i < length; i++) {
        gunichar ch = g_utf8_get_char(text + i);
        if (g_unichar_isdigit(ch) || ch == '.' || ch == ',') {
            filtered[j++] = text[i];
        }
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

gboolean has_infinity(const gchar *s) {
    if (!s) return FALSE;
    return (g_utf8_strchr(s, -1, 0x221E) != NULL) || (strstr(s, "inf") != NULL);
}

gchar* normalize_decimal(const gchar *s) {
    if (!s) return g_strdup("");
    gchar *dup = g_strdup(s);
    for (char *p = dup; *p; ++p) if (*p == ',') *p = '.';
    return dup;
}

gchar* trimdup(const gchar *s) {
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

// ---------ALGORITMO KNAPSACK ----------

/* 0/1 Knapsack */
int knapsack_01(int n, int W, KnapsackItem objs[]) {
    int dp[MAX_OBJ + 1][MAX_CAP + 1];
    memset(dp, 0, sizeof(dp));

    for (int i = 1; i <= n; i++) {
        for (int w = 0; w <= W; w++) {
            dp[i][w] = dp[i-1][w];
            if ((int)objs[i-1].cost <= w) {
                int val = (int)objs[i-1].value + dp[i-1][w - (int)objs[i-1].cost];
                if (val > dp[i][w]) dp[i][w] = val;
            }
        }
    }
    return dp[n][W];
}

/* Bounded Knapsack */
int knapsack_bounded(int n, int W, KnapsackItem objs[]) {
    int dp[MAX_CAP + 1];
    memset(dp, 0, sizeof(dp));

    for (int i = 0; i < n; i++) {
        int maxUnits = objs[i].unbounded ? INT_MAX : objs[i].quantity;
        for (int w = W; w >= 0; w--) {
            for (int k = 1; k <= maxUnits && k * (int)objs[i].cost <= w; k++) {
                int val = k * (int)objs[i].value + dp[w - k * (int)objs[i].cost];
                if (val > dp[w]) dp[w] = val;
            }
        }
    }
    return dp[W];
}

int knapsack_bounded_detailed(int n, int W, KnapsackItem objs[], BoundedCell ***table_ptr) {
    BoundedCell **table = g_new(BoundedCell *, n);
    for (int i = 0; i < n; i++) {
        table[i] = g_new(BoundedCell, W + 1);
        for (int w = 0; w <= W; w++) {
            table[i][w].value = 0;
            memset(table[i][w].counts, 0, sizeof(table[i][w].counts));
        }
    }
    for (int i = 0; i < n; i++) {
        int maxUnits = objs[i].unbounded ? INT_MAX : objs[i].quantity;
        for (int w = 0; w <= W; w++) {
            if (i > 0) {
                table[i][w].value = table[i-1][w].value;
                memcpy(table[i][w].counts, table[i-1][w].counts, sizeof(table[i][w].counts));
            }
            for (int k = 1; k <= maxUnits; k++) {
                int cost_k = k * (int)objs[i].cost;
                if (cost_k > w) break; 
                int remaining = w - cost_k;
                int new_value = k * (int)objs[i].value;
                if (i > 0 && remaining >= 0) {
                    new_value += table[i-1][remaining].value;
                }
                if (new_value > table[i][w].value) {
                    table[i][w].value = new_value;
                    if (i > 0 && remaining >= 0) {
                        memcpy(table[i][w].counts, table[i-1][remaining].counts, sizeof(table[i][w].counts));
                    } else {
                        memset(table[i][w].counts, 0, sizeof(table[i][w].counts));
                    }
                    table[i][w].counts[i] = k;
                }
            }
        }
    }
    
    *table_ptr = table; 
    return table[n-1][W].value;
}

/* Unbounded Knapsack */
int knapsack_unbounded(int n, int W, KnapsackItem objs[]) {
    int dp[MAX_CAP + 1];
    memset(dp, 0, sizeof(dp));

    for (int w = 0; w <= W; w++) {
        for (int i = 0; i < n; i++) {
            if ((int)objs[i].cost <= w) {
                int val = (int)objs[i].value + dp[w - (int)objs[i].cost];
                if (val > dp[w]) dp[w] = val;
            }
        }
    }
    return dp[W];
}

// --------- LATEX --------- 

void compile_latex_file(const gchar *tex_file) {
    gchar *dir = g_path_get_dirname(tex_file);
    gchar *base = g_path_get_basename(tex_file);
    
    gchar *cmd = g_strdup_printf("cd \"%s\" && pdflatex -interaction=nonstopmode \"%s\"", dir, base);
    int result = system(cmd);
    
    if (result == 0) {
        gchar *pdf_file;
        if (g_str_has_suffix(base, ".tex")) {
            gchar *base_name = g_strndup(base, strlen(base) - 4);
            pdf_file = g_strdup_printf("%s/%s.pdf", dir, base_name);
            g_free(base_name);
        } else {
            pdf_file = g_strdup_printf("%s/%s.pdf", dir, base);
        }
        
        if (g_file_test(pdf_file, G_FILE_TEST_EXISTS)) {
            gchar *view_cmd = g_strdup_printf("evince --presentation\"%s\" &", pdf_file);
            system(view_cmd);
            g_free(view_cmd);
        }
        g_free(pdf_file);
    }
    
    g_free(cmd);
    g_free(dir);
    g_free(base);
}

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

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Archivos LaTeX (*.tex)");
    gtk_file_filter_add_pattern(filter, "*.tex");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    gchar *reports_dir = g_build_filename(g_get_current_dir(), "ReportsKnapsack", NULL);
    if (g_file_test(reports_dir, G_FILE_TEST_IS_DIR)) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), reports_dir);
    }
    g_free(reports_dir);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        if (last_selected_tex) g_free(last_selected_tex);
        last_selected_tex = gtk_file_chooser_get_filename(chooser);
        
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
            gchar *edit_cmd = g_strdup_printf("xdg-open \"%s\"", last_selected_tex);
            system(edit_cmd);
            g_free(edit_cmd);
        } else if (choice == GTK_RESPONSE_NO) {
            compile_latex_file(last_selected_tex);
        } else if (choice == GTK_RESPONSE_APPLY) {
            gchar *edit_cmd = g_strdup_printf("xdg-open \"%s\"", last_selected_tex);
            system(edit_cmd);
            g_free(edit_cmd);
            
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
        g_ptr_array_free(entry_costs, TRUE); 
        entry_costs = NULL; 
    }
    if (entry_values) { 
        g_ptr_array_free(entry_values, TRUE); 
        entry_values = NULL; 
    }
    if (entry_quantity) { 
        g_ptr_array_free(entry_quantity, TRUE); 
        entry_quantity = NULL; 
    }

    if (items < 1) items = 1;

    entry_costs = g_ptr_array_sized_new(items);
    entry_values = g_ptr_array_sized_new(items);
    entry_quantity = g_ptr_array_sized_new(items);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_widget_set_vexpand(grid, TRUE);

    // Column Headers
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Object"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Cost"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Value"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Quantity"), 3, 0, 1, 1);

    for (int i = 0; i < items; i++) {
        gchar *name = object_name_setter(i + 1);
        GtkWidget *lbl = gtk_label_new(name);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), lbl, 0, i + 1, 1, 1);

        GtkWidget *cost_cell = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(cost_cell), 8);
        gtk_entry_set_alignment(GTK_ENTRY(cost_cell), 1.0);
        gtk_entry_set_placeholder_text(GTK_ENTRY(cost_cell), "0.0");
        gtk_grid_attach(GTK_GRID(grid), cost_cell, 1, i + 1, 1, 1);
        g_ptr_array_add(entry_costs, cost_cell);
        g_signal_connect(cost_cell, "insert-text", G_CALLBACK(validate_entry), NULL);

        GtkWidget *value_cell = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(value_cell), 8);
        gtk_entry_set_alignment(GTK_ENTRY(value_cell), 1.0);
        gtk_entry_set_placeholder_text(GTK_ENTRY(value_cell), "0.0");
        gtk_grid_attach(GTK_GRID(grid), value_cell, 2, i + 1, 1, 1);
        g_ptr_array_add(entry_values, value_cell);
        g_signal_connect(value_cell, "insert-text", G_CALLBACK(validate_entry), NULL);

        GtkWidget *quantity_cell = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(quantity_cell), 6);
        gtk_entry_set_alignment(GTK_ENTRY(quantity_cell), 1.0);

        if (selected_rb == 1) {
            gtk_entry_set_text(GTK_ENTRY(quantity_cell), "1");
            gtk_editable_set_editable(GTK_EDITABLE(quantity_cell), FALSE);
        } else if (selected_rb == 2) {
            gtk_entry_set_text(GTK_ENTRY(quantity_cell), "1");
            g_signal_connect(quantity_cell, "insert-text", G_CALLBACK(validate_entry), NULL);
        } else if (selected_rb == 3) {
            gtk_entry_set_text(GTK_ENTRY(quantity_cell), "∞");
            gtk_editable_set_editable(GTK_EDITABLE(quantity_cell), FALSE);
        }

        gtk_grid_attach(GTK_GRID(grid), quantity_cell, 3, i + 1, 1, 1);
        g_ptr_array_add(entry_quantity, quantity_cell);
    }

    gtk_container_add(GTK_CONTAINER(scrollWindow), grid);
    gtk_widget_show_all(grid);
    current_grid = grid;
}

GArray* read_knapsack_items(int n_items) {
    if (!entry_costs || !entry_values || !entry_quantity) return NULL;
    if (n_items <= 0) return NULL;

    GArray *items = g_array_new(FALSE, FALSE, sizeof(KnapsackItem));

    for (int i = 0; i < n_items; i++) {
        KnapsackItem obj;
        memset(&obj, 0, sizeof(obj));
        obj.unbounded = FALSE;
        obj.quantity = 0;

        gchar *nm = object_name_setter(i + 1);
        g_strlcpy(obj.name, nm, sizeof(obj.name));

        const gchar *tc_raw = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(entry_costs, i)));
        gchar *tc_norm = normalize_decimal(tc_raw);
        obj.cost = g_ascii_strtod(tc_norm, NULL);
        if (obj.cost < 0) obj.cost = 0;
        g_free(tc_norm);

        const gchar *tv_raw = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(entry_values, i)));
        gchar *tv_norm = normalize_decimal(tv_raw);
        obj.value = g_ascii_strtod(tv_norm, NULL);
        if (obj.value < 0) obj.value = 0;
        g_free(tv_norm);

        const gchar *tq_raw = gtk_entry_get_text(GTK_ENTRY(g_ptr_array_index(entry_quantity, i)));
        gchar *tq = trimdup(tq_raw);

        if (has_infinity(tq)) {
            obj.unbounded = TRUE;
            obj.quantity = -1;
        } else {
            obj.quantity = (int)g_ascii_strtod(tq, NULL);
            if (obj.quantity < 0) obj.quantity = 0;
        }
        g_free(tq);

        g_array_append_val(items, obj);
    }

    return items;
}

void generate_latex_report(int capacity, GArray *items, int max_value, int problem_type, int **dp, int n) {
    g_mkdir_with_parents("ReportsKnapsack", 0755);
    const gchar *custom_name = gtk_entry_get_text(GTK_ENTRY(fileName));
    gchar *filename;
    if (custom_name && strlen(custom_name) > 0) {
        filename = g_strdup_printf("ReportsKnapsack/%s.tex", custom_name);
    } else {
        filename = g_strdup_printf("ReportsKnapsack/knapsack_report_%ld.tex", (long)time(NULL));
    }
    FILE *f = fopen(filename, "w");
    if (!f) {
        g_printerr("Error creating LaTeX file\n");
        g_free(filename);
        return;
    }
    fprintf(f, "\\documentclass{article}\n");
    fprintf(f, "\\usepackage[utf8]{inputenc}\n");
    fprintf(f, "\\usepackage{amsmath}\n");
    fprintf(f, "\\usepackage{xcolor}\n");
    fprintf(f, "\\usepackage{colortbl}\n");
    fprintf(f, "\\usepackage{geometry}\n");
    fprintf(f, "\\usepackage{multirow}\n");
    fprintf(f, "\\usepackage{graphicx}\n");
    fprintf(f, "\\geometry{margin=1in}\n");
    fprintf(f, "\\definecolor{verde}{RGB}{0, 128, 0}\n");
    fprintf(f, "\\definecolor{rojo}{RGB}{255, 0, 0}\n");
    fprintf(f, "\\title{Proyecto 2: El Problema de la Mochila}\n");
    fprintf(f, "\\author{Emily Sanchez \\\\ Viviana Vargas \\\\[1cm] Curso: Investigación de Operaciones \\\\ II Semestre 2025}\n");
    fprintf(f, "\\date{\\today}\n\n");
    fprintf(f, "\\begin{document}\n");
    fprintf(f, "\\maketitle\n\n");
    fprintf(f, "\\thispagestyle{empty}\n");
    fprintf(f, "\\newpage\n");
    fprintf(f, "\\setcounter{page}{1}\n\n");
    fprintf(f, "\\section{Problema de la Mochila (Knapsack Problem)}\n\n");
    fprintf(f, "El \\textbf{problema de la mochila} es un clasico de la "
            "\\textit{optimizacion combinatoria}. Se dispone de una "
            "\\textbf{mochila} con una \\textbf{capacidad maxima} $W$ "
            "y un conjunto de $n$ objetos. Cada objeto $i$ tiene un "
            "\\textbf{peso} $w_i$ y un \\textbf{valor} $v_i$. "
            "El objetivo es seleccionar los objetos de manera que:\n");
    fprintf(f, "\\begin{itemize}\n");
    fprintf(f, "  \\item La suma total de los pesos no exceda la capacidad $W$.\n");
    fprintf(f, "  \\item Se maximice el valor total de los objetos elegidos.\n");
    fprintf(f, "\\end{itemize}\n\n");
    fprintf(f, "\\subsection{Variantes principales}\n");
    fprintf(f, "\\begin{description}\n");
    fprintf(f, "  \\item[0/1 Knapsack] Cada objeto puede elegirse una sola vez o no elegirse: decision binaria.\n");
    fprintf(f, "  \\item[Bounded Knapsack] Cada objeto puede seleccionarse un numero limitado de veces.\n");
    fprintf(f, "  \\item[Unbounded Knapsack] Se permite una cantidad ilimitada de cada objeto.\n");
    fprintf(f, "\\end{description}\n\n");
    fprintf(f, "\\subsection{Solucion}\n");
    fprintf(f, "\\paragraph{0/1 Knapsack} Se resuelve comunmente con "
            "\\textbf{programacion dinamica}. Sea $dp[i][w]$ el valor "
            "maximo al considerar los primeros $i$ objetos y capacidad $w$.\n");
    fprintf(f, "\\[\n"
            "dp[i][w] =\n"
            "\\begin{cases}\n"
            "dp[i-1][w] & \\text{si } w_i > w, \\\\\n"
            "\\max ( dp[i-1][w], v_i + dp[i-1][w - w_i] ) & \\text{si } w_i \\le w.\n"
            "\\end{cases}\n"
            "\\]\n\n");
    fprintf(f, "\\paragraph{Unbounded Knapsack} Similar al 0/1 pero permitiendo repeticiones:\n");
    fprintf(f, "\\[\n"
            "dp[w] = \\max ( dp[w], v_i + dp[w - w_i] ).\n"
            "\\]\n\n");
    fprintf(f, "\\thispagestyle{empty}\n");
    fprintf(f, "\\newpage\n");
    fprintf(f, "\\textbf{Tipo de problema:} ");
    switch(problem_type) {
        case 1: fprintf(f, "0/1 Knapsack\\\\\n"); break;
        case 2: fprintf(f, "Bounded Knapsack\\\\\n"); break;
        case 3: fprintf(f, "Unbounded Knapsack\\\\\n"); break;
    }
    fprintf(f, "\\textbf{Capacidad máxima:} %d\\\\\n", capacity);
    fprintf(f, "\\textbf{Número de objetos:} %d\\\\\n\n", items->len);
    fprintf(f, "\\section*{Datos del Problema}\n");
    fprintf(f, "\\begin{tabular}{|c|c|c|c|}\n");
    fprintf(f, "\\hline\n");
    fprintf(f, "Objeto & Costo & Valor & Cantidad \\\\\n");
    fprintf(f, "\\hline\n");
    
    for (guint i = 0; i < items->len; i++) {
        KnapsackItem *it = &g_array_index(items, KnapsackItem, i);
        fprintf(f, "%s & %.2f & %.2f & ", it->name, it->cost, it->value);
        if (it->unbounded) {
            fprintf(f, "$\\infty$ \\\\\n");
        } else {
            fprintf(f, "%d \\\\\n", it->quantity);
        }
    }
    fprintf(f, "\\hline\n");
    fprintf(f, "\\end{tabular}\n\n");
    fprintf(f, "\\section*{Tabla de Programación Dinámica}\n");
    fprintf(f, "\\begin{center}\n");
    fprintf(f, "\\scriptsize\n");
    fprintf(f, "\\begin{tabular}{|c|");
    for (int i = 1; i <= n; i++) {   
        fprintf(f, "c|");
    }
    fprintf(f, "}\n\\hline\n");
    fprintf(f, "Capacidad/Objetos ");
    for (int i = 1; i <= n; i++) {  
        KnapsackItem *it = &g_array_index(items, KnapsackItem, i-1);
        fprintf(f, "& %s ", it->name);
    }
    fprintf(f, "\\\\ \\hline\n");
    for (int w = 0; w <= capacity; w++) {
        fprintf(f, "%d ", w);
        
        for (int i = 1; i <= n; i++) {   
            gboolean selected = FALSE;
            if (w >= (int)g_array_index(items, KnapsackItem, i-1).cost) {
                int without = dp[i-1][w];
                int with = dp[i-1][w - (int)g_array_index(items, KnapsackItem, i-1).cost] +
                        (int)g_array_index(items, KnapsackItem, i-1).value;
                
                if (with > without) {
                    selected = TRUE;
                }
            }
            if (selected) {
                fprintf(f, "& \\cellcolor{verde}\\textcolor{white}{%d} ", dp[i][w]);
            } else {
                fprintf(f, "& \\cellcolor{rojo}\\textcolor{white}{%d} ", dp[i][w]);
            }
        }
        fprintf(f, "\\\\ \\hline\n");
    }
    fprintf(f, "\\end{tabular}\n");
    fprintf(f, "\\end{center}\n");
    fprintf(f, "\\normalsize\n\n");
    fprintf(f, "\\section*{Solución Óptima}\n");
    fprintf(f, "\\textbf{Valor máximo obtenido:} %d\\\\\n", max_value);
    fprintf(f, "\\textbf{Objetos seleccionados:} ");
    int w = capacity;
    gboolean first = TRUE;
    for (int i = n; i > 0; i--) {
        if (dp[i][w] != dp[i-1][w]) {
            KnapsackItem *it = &g_array_index(items, KnapsackItem, i-1);
            if (!first) {
                fprintf(f, ", ");
            }
            fprintf(f, "%s", it->name);
            w -= (int)it->cost;
            first = FALSE;
        }
    }
    if (first) {
        fprintf(f, "Ninguno");
    }
    fprintf(f, "\\\\\n");
    fprintf(f, "\\textbf{Capacidad utilizada:} %d\\\\\n", capacity - w);
    fprintf(f, "\\end{document}\n");
    fclose(f);
    compile_latex_file(filename);
    g_free(filename);
}

void generate_bounded_latex_report(int capacity, GArray *items, int max_value, void *bounded_table_ptr, int n, int problem_type) {
    typedef struct {
        int value;
        int counts[MAX_OBJ];
    } BoundedCell;
    
    BoundedCell **table = (BoundedCell **)bounded_table_ptr;

    g_mkdir_with_parents("ReportsKnapsack", 0755);
    
    const gchar *custom_name = gtk_entry_get_text(GTK_ENTRY(fileName));
    gchar *filename;
    
    if (custom_name && strlen(custom_name) > 0) {
        filename = g_strdup_printf("ReportsKnapsack/%s.tex", custom_name);
    } else {
        filename = g_strdup_printf("ReportsKnapsack/knapsack_report_%ld.tex", (long)time(NULL));
    }
    
    FILE *f = fopen(filename, "w");
    
    if (!f) {
        g_printerr("Error creating LaTeX file\n");
        g_free(filename);
        return;
    }
    fprintf(f, "\\documentclass{article}\n");
    fprintf(f, "\\usepackage[utf8]{inputenc}\n");
    fprintf(f, "\\usepackage{amsmath}\n");
    fprintf(f, "\\usepackage{xcolor}\n");
    fprintf(f, "\\usepackage{colortbl}\n");
    fprintf(f, "\\usepackage{geometry}\n");
    fprintf(f, "\\usepackage{multirow}\n");
    fprintf(f, "\\usepackage{graphicx}\n");
    fprintf(f, "\\geometry{margin=1in}\n");
    fprintf(f, "\\definecolor{verde}{RGB}{0, 128, 0}\n");
    fprintf(f, "\\definecolor{rojo}{RGB}{255, 0, 0}\n");
    fprintf(f, "\\title{Proyecto 2: El Problema de la Mochila}\n");
    fprintf(f, "\\author{Emily Sanchez \\\\ Viviana Vargas \\\\[1cm] Curso: Investigación de Operaciones \\\\ II Semestre 2025}\n");
    fprintf(f, "\\date{\\today}\n\n");
    fprintf(f, "\\begin{document}\n");
    fprintf(f, "\\maketitle\n\n");
    fprintf(f, "\\thispagestyle{empty}\n");
    fprintf(f, "\\newpage\n");
    fprintf(f, "\\setcounter{page}{1}\n\n");
    fprintf(f, "\\section{Problema de la Mochila (Knapsack Problem)}\n\n");
    fprintf(f, "El \\textbf{problema de la mochila} es un clasico de la "
            "\\textit{optimizacion combinatoria}. Se dispone de una "
            "\\textbf{mochila} con una \\textbf{capacidad maxima} $W$ "
            "y un conjunto de $n$ objetos. Cada objeto $i$ tiene un "
            "\\textbf{peso} $w_i$ y un \\textbf{valor} $v_i$. "
            "El objetivo es seleccionar los objetos de manera que:\n");
    fprintf(f, "\\begin{itemize}\n");
    fprintf(f, "  \\item La suma total de los pesos no exceda la capacidad $W$.\n");
    fprintf(f, "  \\item Se maximice el valor total de los objetos elegidos.\n");
    fprintf(f, "\\end{itemize}\n\n");
    fprintf(f, "\\subsection{Variantes principales}\n");
    fprintf(f, "\\begin{description}\n");
    fprintf(f, "  \\item[0/1 Knapsack] Cada objeto puede elegirse una sola vez o no elegirse: decision binaria.\n");
    fprintf(f, "  \\item[Bounded Knapsack] Cada objeto puede seleccionarse un numero limitado de veces.\n");
    fprintf(f, "  \\item[Unbounded Knapsack] Se permite una cantidad ilimitada de cada objeto.\n");
    fprintf(f, "\\end{description}\n\n");
    fprintf(f, "\\subsection{Solucion}\n");
    fprintf(f, "\\paragraph{0/1 Knapsack} Se resuelve comunmente con "
            "\\textbf{programacion dinamica}. Sea $dp[i][w]$ el valor "
            "maximo al considerar los primeros $i$ objetos y capacidad $w$.\n");
    fprintf(f, "\\[\n"
            "dp[i][w] =\n"
            "\\begin{cases}\n"
            "dp[i-1][w] & \\text{si } w_i > w, \\\\\n"
            "\\max ( dp[i-1][w], v_i + dp[i-1][w - w_i] ) & \\text{si } w_i \\le w.\n"
            "\\end{cases}\n"
            "\\]\n\n");
    fprintf(f, "\\paragraph{Unbounded Knapsack} Similar al 0/1 pero permitiendo repeticiones:\n");
    fprintf(f, "\\[\n"
            "dp[w] = \\max ( dp[w], v_i + dp[w - w_i] ).\n"
            "\\]\n\n");
    fprintf(f, "\\thispagestyle{empty}\n");
    fprintf(f, "\\newpage\n");
    fprintf(f, "\\textbf{Tipo de problema:} ");
    switch(problem_type) {
        case 1: fprintf(f, "0/1 Knapsack\\\\\n"); break;
        case 2: fprintf(f, "Bounded Knapsack\\\\\n"); break;
        case 3: fprintf(f, "Unbounded Knapsack\\\\\n"); break;
    }
    fprintf(f, "\\textbf{Capacidad máxima:} %d\\\\\n", capacity);
    fprintf(f, "\\textbf{Número de objetos:} %d\\\\\n\n", items->len);
    fprintf(f, "\\section*{Datos del Problema}\n");
    fprintf(f, "\\begin{tabular}{|c|c|c|c|}\n");
    fprintf(f, "\\hline\n");
    fprintf(f, "Objeto & Costo & Valor & Cantidad \\\\\n");
    fprintf(f, "\\hline\n");
    for (guint i = 0; i < items->len; i++) {
        KnapsackItem *it = &g_array_index(items, KnapsackItem, i);
        fprintf(f, "%s & %.2f & %.2f & ", it->name, it->cost, it->value);
        if (it->unbounded) {
            fprintf(f, "$\\infty$ \\\\\n");
        } else {
            fprintf(f, "%d \\\\\n", it->quantity);
        }
    }
    fprintf(f, "\\hline\n");
    fprintf(f, "\\end{tabular}\n\n");
    fprintf(f, "\\section*{Tabla de Programación Dinámica Detallada}\n");
    fprintf(f, "\\begin{center}\n");
    fprintf(f, "\\scriptsize\n");
    fprintf(f, "\\begin{tabular}{|c|");
    for (int i = 0; i < n; i++) {
        fprintf(f, "c|");
    }
    fprintf(f, "}\n\\hline\n");
    fprintf(f, "Capacidad ");
    for (int i = 0; i < n; i++) {
        KnapsackItem *it = &g_array_index(items, KnapsackItem, i);
        fprintf(f, "& %s ", it->name);
    }
    fprintf(f, "\\\\ \\hline\n");
    for (int w = 0; w <= capacity; w++) {
        fprintf(f, "%d ", w);
        for (int i = 0; i < n; i++) {
            int count = table[i][w].counts[i];
            int current_value = table[i][w].value;
            if (count > 0) {
                fprintf(f, "& \\cellcolor{verde}\\textcolor{white}{%d(%d)} ",
                        current_value, count);
            } else {
                fprintf(f, "& \\cellcolor{rojo}\\textcolor{white}{%d} ",
                        current_value);
            }
        }
        fprintf(f, "\\\\ \\hline\n");
    }
    fprintf(f, "\\end{tabular}\n");
    fprintf(f, "\\end{center}\n");
    fprintf(f, "\\normalsize\n\n");
    fprintf(f, "\\section*{Solución Óptima}\n");
    fprintf(f, "\\textbf{Valor máximo obtenido:} %d\\\\\n", max_value);
    fprintf(f, "\\textbf{Objetos seleccionados:} ");
    gboolean first = TRUE;
    int current_w = capacity;
    
    for (int i = n-1; i >= 0; i--) {
        int count = table[i][current_w].counts[i];
        if (count > 0) {
            KnapsackItem *it = &g_array_index(items, KnapsackItem, i);
            if (!first) {
                fprintf(f, ", ");
            }
            fprintf(f, "%s:%d", it->name, count);
            current_w -= count * (int)it->cost;
            first = FALSE;
        }
    }
    
    if (first) {
        fprintf(f, "Ninguno");
    }
    fprintf(f, "\\\\\n");
    fprintf(f, "\\textbf{Capacidad utilizada:} %d\\\\\n", capacity - current_w);
    fprintf(f, "\\end{document}\n");
    fclose(f);
    compile_latex_file(filename);
    g_free(filename);
}

// --------- BOTONES --------- 

G_MODULE_EXPORT void on_objects_value_changed(GtkSpinButton *objects, gpointer user_data) {
    int n = gtk_spin_button_get_value_as_int(objects);
    build_table(n);
}

G_MODULE_EXPORT void on_rb_01_toggled(GtkRadioButton *rb_01, gpointer user_data) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_01))) {
        selected_rb = 1;
        int n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(objects));
        build_table(n);
    }
}

G_MODULE_EXPORT void on_rb_bounded_toggled(GtkRadioButton *rb_bounded, gpointer user_data) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_bounded))) {
        selected_rb = 2;
        int n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(objects));
        build_table(n);
    }
}

G_MODULE_EXPORT void on_rb_unbounded_toggled(GtkRadioButton *rb_unbounded, gpointer user_data) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_unbounded))) {
        selected_rb = 3;
        int n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(objects));
        build_table(n);
    }
}

G_MODULE_EXPORT void on_fileLoad_file_set(GtkWidget *loadProblem, gpointer data) {
    gtk_widget_set_sensitive(loadToGrid, TRUE);
    gtk_widget_set_sensitive(saveProblem, TRUE);
}

G_MODULE_EXPORT void on_exitButton_clicked(GtkButton *exitButton1, gpointer data) {
    gtk_main_quit();
}

G_MODULE_EXPORT void on_editLatexButton_clicked(GtkWidget *editLatex, gpointer data) {
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
            gchar *edit_cmd = g_strdup_printf("xdg-open \"%s\"", last_selected_tex);
            system(edit_cmd);
            g_free(edit_cmd);
            return;
        }
    }
    
    on_select_latex_file(editLatex, data);
}

G_MODULE_EXPORT void on_createSolution_clicked(GtkWidget *btn, gpointer data) {
    int capacity = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(maxCapacity));
    int n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(objects));
    
    GArray *items = read_knapsack_items(n);
    if (!items) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window1),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Error: No se pudieron leer los items");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    int max_value = 0;
        if (selected_rb == 2) {
        BoundedCell **bounded_table;
        max_value = knapsack_bounded_detailed(n, capacity, (KnapsackItem *)items->data, &bounded_table);
        generate_bounded_latex_report(capacity, items, max_value, (void *)bounded_table, n, selected_rb);
        for (int i = 0; i < n; i++) {
            g_free(bounded_table[i]);
        }
        g_free(bounded_table);
    } else if (selected_rb == 3) {
        BoundedCell **bounded_table;
        max_value = knapsack_bounded_detailed(n, capacity, (KnapsackItem *)items->data, &bounded_table);
        generate_bounded_latex_report(capacity, items, max_value, (void *)bounded_table, n, selected_rb);
        for (int i = 0; i < n; i++) {
            g_free(bounded_table[i]);
        }
        g_free(bounded_table);
    } else {
        int **dp = g_new(int *, n + 1);
        for (int i = 0; i <= n; i++) {
            dp[i] = g_new(int, capacity + 1);
            for (int w = 0; w <= capacity; w++) {
                if (i == 0 || w == 0) {
                    dp[i][w] = 0;
                } else if ((int)g_array_index(items, KnapsackItem, i-1).cost <= w) {
                    KnapsackItem *it = &g_array_index(items, KnapsackItem, i-1);
                    int without = dp[i-1][w];
                    int with = (int)it->value + dp[i-1][w - (int)it->cost];
                    dp[i][w] = (with > without) ? with : without;
                } else {
                    dp[i][w] = dp[i-1][w];
                }
            }
        }

        switch(selected_rb) {
            case 1: max_value = knapsack_01(n, capacity, (KnapsackItem *)items->data); break;
            case 3: max_value = knapsack_unbounded(n, capacity, (KnapsackItem *)items->data); break;
        }

        generate_latex_report(capacity, items, max_value, selected_rb, dp, n);

        for (int i = 0; i <= n; i++) {
            g_free(dp[i]);
        }
        g_free(dp);
    }
    
    g_array_free(items, TRUE);
}

G_MODULE_EXPORT void on_loadToGrid_clicked(GtkWidget *loadToGrid, gpointer data) {
    // FALTAAAAA
}

G_MODULE_EXPORT void on_saveProblem_clicked(GtkWidget *saveProblem, gpointer data) {
    // FALTAAA
}

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

    g_signal_connect(editLatexButton, "clicked", G_CALLBACK(on_editLatexButton_clicked), NULL);
    g_signal_connect(exitButton1, "clicked", G_CALLBACK(on_exitButton_clicked), NULL);
    g_signal_connect(createSolution, "clicked", G_CALLBACK(on_createSolution_clicked), NULL);
    g_signal_connect(loadToGrid, "clicked", G_CALLBACK(on_loadToGrid_clicked), NULL);
    g_signal_connect(saveProblem, "clicked", G_CALLBACK(on_saveProblem_clicked), NULL);
    g_signal_connect(fileLoad, "file-set", G_CALLBACK(on_fileLoad_file_set), NULL);
    g_signal_connect(objects, "value-changed", G_CALLBACK(on_objects_value_changed), NULL);
    gtk_widget_set_sensitive(loadToGrid, FALSE);
    gtk_widget_set_sensitive(saveProblem, FALSE);
    
    build_table( gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(objects)) );
    gtk_widget_show(window1);
    
    gtk_main();

    return EXIT_SUCCESS;
}
