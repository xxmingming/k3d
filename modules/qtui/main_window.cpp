// K-3D
// Copyright (c) 1995-2006, Timothy M. Shead
//
// Contact: tshead@k-3d.com
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your argument) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

/** \file
	\author Tim Shead (tshead@k-3d.com)
*/

#include "main_window.h"
#include "scene.h"

#include <k3d-i18n-config.h>
#include <k3dsdk/application.h>
#include <k3dsdk/classes.h>
#include <k3dsdk/iapplication.h>
#include <k3dsdk/idocument_importer.h>
#include <k3dsdk/node.h>
#include <k3dsdk/plugin.h>
#include <k3dsdk/qtui/document.h>
#include <k3dsdk/share.h>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

#include <boost/scoped_ptr.hpp>

#include <iomanip>

namespace module
{

namespace qtui
{

//////////////////////////////////////////////////////////////////////////
// main_window
	
main_window::main_window(k3d::idocument& Document) :
	m_document(Document)
{
k3d::log() << debug << __PRETTY_FUNCTION__ << std::endl;

	ui.setupUi(this);
	statusBar()->setVisible(false);
	setAttribute(Qt::WA_DeleteOnClose);

	m_scene.reset(new scene(Document));
	ui.viewport->setScene(m_scene.get());

	connect(ui.actionNew, SIGNAL(activated()), this, SLOT(on_file_new()));
	connect(ui.actionOpen, SIGNAL(activated()), this, SLOT(on_file_open()));
	connect(ui.actionQuit, SIGNAL(activated()), QCoreApplication::instance(), SLOT(quit()));
}

main_window::main_window(const k3d::filesystem::path& DocumentPath, k3d::idocument& Document) :
	m_document(Document)
{
k3d::log() << debug << __PRETTY_FUNCTION__ << std::endl;

	ui.setupUi(this);
	statusBar()->setVisible(false);
	setAttribute(Qt::WA_DeleteOnClose);

	setWindowTitle(("K-3D - " + DocumentPath.leaf().raw()).c_str());

	m_scene.reset(new scene(Document));
	ui.viewport->setScene(m_scene.get());

	connect(ui.actionNew, SIGNAL(activated()), this, SLOT(on_file_new()));
	connect(ui.actionOpen, SIGNAL(activated()), this, SLOT(on_file_open()));
	connect(ui.actionQuit, SIGNAL(activated()), QCoreApplication::instance(), SLOT(quit()));
}

main_window::~main_window()
{
k3d::log() << debug << __PRETTY_FUNCTION__ << std::endl;
}

void main_window::on_file_new()
{
	k3d::idocument* const document = k3d::application().create_document();
	return_if_fail(document);

	k3d::qtui::populate_new_document(*document);

	main_window* const window = new main_window(*document);
	window->show();
}

void main_window::on_file_open()
{
	boost::scoped_ptr<k3d::idocument_importer> importer(k3d::plugin::create<k3d::idocument_importer>(k3d::classes::DocumentImporter()));
	if(!importer.get())
	{
		QMessageBox::warning(this, _("Open K-3D Document:"), _("Document reader plugin not installed."), QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton);
		return;
	}

	const QString filepath = QFileDialog::getOpenFileName(this, _("Choose a file to open:"), "/home", _("K-3D Documents (*.k3d)"));
	if(filepath.isEmpty())
		return;

	const k3d::filesystem::path document_path = k3d::filesystem::native_path(k3d::ustring::from_utf8(filepath.toAscii().data()));

	k3d::idocument* const document = k3d::application().create_document();
	return_if_fail(document);

	if(!importer->read_file(document_path, *document))
	{
		QMessageBox::warning(this, _("Open K-3D Document:"), _("Error reading document."), QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton);
		return;
	}

	main_window* const window = new main_window(document_path, *document);
	window->show();
}

} // namespace qtui

} // namespace module

