struct CustomColor color_fl_accountName={N_("Account Name: "), "fl_accountName", 1};
struct CustomColor color_fl_accountType={N_("Account Type: "), "fl_accountType", 2};
struct CustomColor color_fl_folderName={N_("Folder Name: "), "fl_folderName", 3};
struct CustomColor color_fl_folderDirName={N_("Directory Name: "), "fl_folderDirName", 4};
struct CustomColor color_fl_messageCount={N_("Message Count: "), "fl_messageCount", 5};
struct CustomColor color_md_quote1={N_("Quote Level 1: "), "md_quote1", 1};
struct CustomColor color_md_quote2={N_("Quote Level 2: "), "md_quote2", 2};
struct CustomColor color_md_quote3={N_("Quote Level 3: "), "md_quote3", 3};
struct CustomColor color_md_quote4={N_("Quote Level 4: "), "md_quote4", 4};
struct CustomColor color_md_headerName={N_("Header Name: "), "md_headerName", 5};
struct CustomColor color_md_headerContents={N_("Header Contents: "), "md_headerContents", 6};
struct CustomColor color_md_formatWarning={N_("Format warnings: "), "md_formatWarning", 7};
struct CustomColor color_wm_headerName={N_("Header Name: "), "wm_headerName", 1};
struct CustomColor color_perms_user={N_("User Permissions: "), "perms_user", 1};
struct CustomColor color_perms_group={N_("Group Permissions: "), "perms_group", 2};
struct CustomColor color_perms_owner={N_("Owner Permissions: "), "perms_owner", 3};
struct CustomColor color_perms_admins={N_("Administrator Permissions: "), "perms_admins", 4};
struct CustomColor color_perms_other={N_("Other Permissions: "), "perms_other", 5};
struct CustomColor color_perms_rights={N_("Edit Individual Permissions: "), "perms_rights", 6};
struct CustomColor color_misc_promptColor={N_("Prompt: "), "misc_promptColor", 1};
struct CustomColor color_misc_inputField={N_("Prompt Field: "), "misc_inputField", 2};
struct CustomColor color_misc_buttonColor={N_("Action Button: "), "misc_buttonColor", 3};
struct CustomColor color_misc_menuTitle={N_("Menu Title: "), "misc_menuTitle", 4};
struct CustomColor color_misc_menuOption={N_("Menu Option: "), "misc_menuOption", 5};
struct CustomColor color_misc_titleBar={N_("Title Line: "), "misc_titleBar", 6};
struct CustomColor color_misc_statusBar={N_("Status Line: "), "misc_statusBar", 7};
struct CustomColor color_misc_hotKey={N_("Hot Key: "), "misc_hotKey", 8};
struct CustomColor color_misc_hotKeyDescr={N_("Hot Key Description: "), "misc_hotKeyDescr", 9};

static struct CustomColor *messageDisplay[]={
	&color_md_quote1,
	&color_md_quote2,
	&color_md_quote3,
	&color_md_quote4,
	&color_md_headerName,
	&color_md_headerContents,
	&color_md_formatWarning,
	NULL};

static struct CustomColor *writeMessage[]={
	&color_wm_headerName,
	NULL};

static struct CustomColor *perms[]={
	&color_perms_user,
	&color_perms_group,
	&color_perms_owner,
	&color_perms_admins,
	&color_perms_other,
	&color_perms_rights,
	NULL};

static struct CustomColor *miscColors[]={
	&color_misc_promptColor,
	&color_misc_inputField,
	&color_misc_buttonColor,
	&color_misc_menuTitle,
	&color_misc_menuOption,
	&color_misc_titleBar,
	&color_misc_statusBar,
	&color_misc_hotKey,
	&color_misc_hotKeyDescr,
	NULL};

static struct CustomColor *folderListing[]={
	&color_fl_accountName,
	&color_fl_accountType,
	&color_fl_folderName,
	&color_fl_folderDirName,
	&color_fl_messageCount,
	NULL};

static struct CustomColorGroup allGroups[]={
	{N_("Folder Listing Screen Colors: "), folderListing},
	{N_("Message Display Colors: "), messageDisplay},
	{N_("Write Message Colors: "), writeMessage},
	{N_("Folder Permissions Screen Colors: "), perms},
	{N_("Miscellaneous Colors: "), miscColors},
	{"", NULL}};
