#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include "config.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#define _(x) gettext(x)
#else
#define _(x) (x)
#endif

#ifdef __linux__

#include "../cfg.c"
#include <sys/ioctl.h>

GtkWidget *MainWindow;

// function to check if the device is a cdrom
int is_cdrom(const char *device) {
	struct stat st;
	int fd = -1;

	// check if the file exist
	if (stat(device, &st) < 0) return 0;

	// check if is a block or char device
	if (!S_ISCHR(st.st_mode) && !S_ISBLK(st.st_mode)) return 0;

	// try to open the device file descriptor
	if ((fd = open(device, O_RDONLY | O_NONBLOCK)) < 0) return 0;

	// I need a method to check is a device is really a CD-ROM.
	// some problems/ideas are:
	// - different protocls (ide, scsi, old proprietary...)
	// - maybe we can use major number (see linux/major.h) to do some check.
	//   major number can be retrieved with (st.st_rdev>>8)
	//   scsi has SCSI_CDROM_MAJOR but does this cover all scsi drives?
	//   beside IDE major is the same for hard disks and cdroms...
	//   and DVDs?
	// - another idea is to parse /proc, but again IDE, scsi etc have 
	//   different files... I've not found a way to query "which CD drives
	//   are available?"
	//
	// Now I use this ioctl which works also if the drive is empty,
	// I hope that is implemented for all the drives... here works
	// fine: at least doesn't let me to select my HD as CDs ;)

	// try a ioctl to see if it's a CD-ROM device
	if (ioctl(fd, CDROM_GET_CAPABILITY, NULL) < 0) {
		close(fd);
		return 0;
	}

	close(fd);

	// yes, it seems a CD drive!
	return 1;
}

// fill_drives_list: retrieves available cd drives. At the moment it use a quite
// ugly "brute force" method: we check for the most common location for cdrom
// in /dev and chech if they are cdrom devices.
// If your cdrom path is not listed here you'll have to type it in the dialog
// entry yourself (or add it here and recompile).
// Are there any other common entry to add to the list? (especially scsi, I
// deliberately ignored old non standard cdroms... )
// If you come up with a better method let me know!!
void fill_drives_list(GtkWidget *widget) {
	int i = 0;
	GtkListStore *store;
	GtkTreeIter iter;

	static const char *cdrom_devices[] = {
		"/dev/cdrom",
		"/dev/cdrom0",
		"/dev/cdrom1",
		"/dev/cdrom2",
		"/dev/cdrom3",
		"/dev/cdroms/cdrom0",
		"/dev/cdroms/cdrom1",
		"/dev/cdroms/cdrom2",
		"/dev/cdroms/cdrom3",
		"/dev/hda",
		"/dev/hdb",
		"/dev/hdc",
		"/dev/hdd",
		"/dev/sda",
		"/dev/sdb",
		"/dev/sdc",
		"/dev/sdd",
		"/dev/scd0",
		"/dev/scd1",
		"/dev/scd2",
		"/dev/scd3",
		"/dev/optcd",
		NULL};

	store = gtk_list_store_new(1, G_TYPE_STRING);

	// first we put our current drive
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, CdromDev, -1);

	// scan cdrom_devices for real cdrom and add them to list
	while (cdrom_devices[i] != NULL) {
		// check that is not our current dev (already in list)
		if (strcmp(cdrom_devices[i], CdromDev) != 0) {
			// check that is a cdrom device
			if (is_cdrom(cdrom_devices[i])) {
				gtk_list_store_append(store, &iter);
				gtk_list_store_set(store, &iter, 0, cdrom_devices[i], -1);
			}
		}
		++i;
	}

	gtk_combo_box_set_model(GTK_COMBO_BOX(widget), GTK_TREE_MODEL(store));
	gtk_combo_box_entry_set_text_column(GTK_COMBO_BOX_ENTRY(widget), 0);
}

static void OnConfigExit(GtkWidget *widget, gpointer user_data) {
	GladeXML *xml;

	xml = glade_get_widget_tree(MainWindow);

	widget = glade_xml_get_widget(xml, "cddev_comboboxentry");
	strncpy(CdromDev, gtk_entry_get_text(GTK_ENTRY(GTK_BIN(widget)->child)), 255);
	CdromDev[255] = '\0';

	widget = glade_xml_get_widget(xml, "readmode_combobox");
	ReadMode = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

	widget = glade_xml_get_widget(xml, "subQ_button");
	UseSubQ = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

	widget = glade_xml_get_widget(xml, "spinCacheSize");
	CacheSize = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

	widget = glade_xml_get_widget(xml, "spinCdrSpeed");
	CdrSpeed = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

	widget = glade_xml_get_widget(xml, "comboSpinDown");
	SpinDown = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

	SaveConf();

	gtk_widget_destroy(widget);
	gtk_exit(0);
}

long CDRconfigure() {
	GladeXML *xml;
	GtkWidget *widget;

	LoadConf();

	xml = glade_xml_new(DATADIR "dfcdrom.glade2", "CfgWnd", NULL);
	if (xml == NULL) {
		g_warning("We could not load the interface!");
		return -1;
	}

	MainWindow = glade_xml_get_widget(xml, "CfgWnd");
	gtk_window_set_title(GTK_WINDOW(MainWindow), _("CDR configuration"));

	widget = glade_xml_get_widget(xml, "CfgWnd");
	g_signal_connect_data(GTK_OBJECT(widget), "delete_event",
		GTK_SIGNAL_FUNC(OnConfigExit), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "cfg_closebutton");
	g_signal_connect_data(GTK_OBJECT(widget), "clicked",
		GTK_SIGNAL_FUNC(OnConfigExit), NULL, NULL, G_CONNECT_AFTER);

	widget = glade_xml_get_widget(xml, "cddev_comboboxentry");
	fill_drives_list(widget);
	gtk_entry_set_text(GTK_ENTRY(GTK_BIN(widget)->child), CdromDev);

	widget = glade_xml_get_widget(xml, "readmode_combobox");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), ReadMode);

	widget = glade_xml_get_widget(xml, "subQ_button");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), UseSubQ);

	widget = glade_xml_get_widget(xml, "spinCacheSize");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), (float)CacheSize);

	widget = glade_xml_get_widget(xml, "spinCdrSpeed");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), (float)CdrSpeed);

	widget = glade_xml_get_widget(xml, "comboSpinDown");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), SpinDown);

	gtk_widget_show(MainWindow);
	gtk_main();

	return 0;
}

static void OnAboutExit(GtkWidget *widget, gpointer user_data) {
	gtk_widget_destroy(widget);
	gtk_exit(0);
}

void CDRabout() {
	GtkWidget *widget;
	const char *authors[]= {"linuzappz <linuzappz@hotmail.com>",
							"xobro <_xobro_@tin.it>", NULL};

	widget = gtk_about_dialog_new();
	gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(widget), "CD-ROM Device Reader");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(widget), "1.0");
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(widget), authors);
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(widget), "http://pcsxr.codeplex.com/");

	g_signal_connect_data(GTK_OBJECT(widget), "response",
		GTK_SIGNAL_FUNC(OnAboutExit), NULL, NULL, G_CONNECT_AFTER);

	gtk_widget_show(widget);
	gtk_main();
}

#endif

int main(int argc, char *argv[]) {
#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain(GETTEXT_PACKAGE, LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

#ifdef __linux__
	gtk_set_locale();
	gtk_init(&argc, &argv);

	if (argc != 2) return 0;

	if (strcmp(argv[1], "configure") == 0) {
		CDRconfigure();
	} else {
		CDRabout();
	}
#endif

	return 0;
}
