/******************************************************************************
 *                         DATA DISPLAY APPLICATION X                         *
 *                            2B TECHNOLOGIES, INC.                           *
 *                                                                            *
 * The DDX is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU General Public License as published by the Free       *
 * Software Foundation, either version 3 of the License, or (at your option)  *
 * any later version.  The DDX is distributed in the hope that it will be     *
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General  *
 * Public License for more details.  You should have received a copy of the   *
 * GNU General Public License along with the DDX.  If not, see                *
 * <http://www.gnu.org/licenses/>.                                            *
 *                                                                            *
 *  For more information about the DDX, check out the 2B website or GitHub:   *
 *       <http://twobtech.com/DDX>       <https://github.com/2BTech/DDX>      *
 ******************************************************************************/

#include "unitmanager.h"
#include "modules/module_register.cpp"

UnitManager::UnitManager(Daemon *parent) : QObject(parent)
{
	QList<QMetaObject> units;
	// Load Modules
	units = registerModules();
	for (int i=0; i < units.size(); i++)
		modules->insert(QString::fromLatin1(units.at(i).className()),
						units.at(i));
	
}

UnitManager::~UnitManager()
{
	
}

bool UnitManager::doesModuleExist(const QString type) const {
	return modules->contains(type);
}

Module* UnitManager::constructModule(const QString type, Path *parent, const QString name) const {
	return (Module*) modules->value(type).newInstance(Q_ARG(Path*, parent),
													  Q_ARG(QString, name));
}
