#include <windows.h>
#include <string>
#include <vector>
#include <stdio.h>
#include "drives.h"
#include "guiutils.h"
#include "FileUtils.h"
#include "fuse.h"

void fatal(HWND hwnd, const char *msg)
{
	MessageBox(hwnd, msg, "EncFS", MB_ICONERROR);
}

#define FATAL(msg) do { fatal(hwnd, msg); return; } while(0)

Drive::Drive(const std::string& _dir, char drive):
	dir(_dir), mounted(false)
{
	sprintf(mnt, "%c:\\", drive);
}

void Drive::Show(HWND hwnd)
{
	ShellExecute(hwnd, "open", mnt, NULL, NULL, SW_SHOWNORMAL);
}

void Drive::Mount(HWND hwnd)
{
	// check drive empty or require a new drive
	while (GetDriveType(mnt) != DRIVE_NO_ROOT_DIR) {
		char drive = SelectFreeDrive(hwnd);
		if (!drive)
			return;
		sprintf(mnt, "%c:\\", drive);
		Save();
	}

	// check directory existence
	if (!isDirectory(dir.c_str())) {
		if (YesNo(hwnd, "Directory does not exists. Remove from list?"))
			Drives::Delete(shared_from_this());
		return;
	}

	// TODO check configuration still exists ?? ... no can cause recursion problem

	// ask a password to mount
	char pass[128];
	if (!GetPassword(hwnd, pass, sizeof(pass)))
		return;

	// TODO mount
	FATAL("not implemented");
	memset(pass, 0, sizeof(pass));
}

void Drive::Umount(HWND hwnd)
{
	// check mounted
	if (GetDriveType(mnt) == DRIVE_NO_ROOT_DIR)
		mounted = false;
	if (!mounted)
		FATAL("Cannot unmount a not mounted drive");

	// unmount
	fuse_unmount(mnt, NULL);
}

void Drive::Save()
{
	// TODO
}

// DRIVES

typedef std::vector<Drives::drive_t> drives_t;

static drives_t drives;

Drives::drive_t Drives::GetDrive(int n)
{
	if (n < 0 || (unsigned) n >= drives.size())
		return boost::shared_ptr<Drive>();
	return drives[n];
}

void Drives::Load()
{
	// TODO
}

void Drives::Save()
{
	// TODO
}

void Drives::AddMenus(HMENU menu, bool mounted, unsigned count, const char *fmt, const char *title, int type)
{
	if (!count)
		return;

	HMENU addMenu = menu;
	if (count > 2)
		addMenu = CreatePopupMenu();

	unsigned n = 0;
	for (drives_t::const_iterator i = drives.begin(); i != drives.end(); ++i, ++n) {
		if (((*i)->mounted && !mounted) || (!(*i)->mounted && mounted))
			continue;

		char buf[512];
		_snprintf(buf, sizeof(buf), count > 2 ? "%s (%c)" : fmt, (*i)->dir.c_str(), (*i)->mnt[0]);
		AppendMenu(addMenu, 0, IDM_MOUNT_ID(n, type), buf);
	}

	if (count > 2)
		AppendMenu(menu, MF_POPUP, (UINT_PTR) addMenu, title);
	else
		AppendMenu(menu, MF_MENUBREAK, -1, NULL);
}

void Drives::AddMenus(HMENU menu)
{
	// TODO check all drives, delete closed processed and so on

	// counts
	unsigned numMounted = 0, numUnmounted = 0;
	for (drives_t::const_iterator i = drives.begin(); i != drives.end(); ++i) {
		if ((*i)->mounted)
			++numMounted;
		else
			++numUnmounted;
	}

	AddMenus(menu, true, numMounted, "Open %s (%c)", "Open", IDM_TYPE_SHOW);
	AddMenus(menu, false, numUnmounted, "Mount %s (%c)", "Mount", IDM_TYPE_MOUNT);
	AddMenus(menu, true, numMounted, "Unmount %s (%c)", "Unmount", IDM_TYPE_UMOUNT);
}

void Drives::Delete(drive_t drive)
{
	drives.erase(std::remove(drives.begin(), drives.end(), drive), drives.end());
	// TODO delete from configuration!
}

Drives::drive_t Drives::Add(const std::string& dir, char drive)
{
	// TODO remove old drive with same directory (or update drive ?? or give error ??)
	Drives::drive_t ret(new Drive(dir, drive));
	drives.push_back(ret);
	ret->Save();
	return ret;
}

