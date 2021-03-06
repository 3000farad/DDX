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

#ifndef DATA_H
#define DATA_H

#include <QObject>
#include <QList>
#include <QString>

/*!
 * \file data.h
 * Data classes for daemons
 * 
 * \ingroup daemon
 */

class Module;

/*!
 * \brief Stores the contents of and metadata about a column as it resides in a stream
 */
struct Column {
	QByteArray c;  //!< The column's actual data buffer
	QString n; //!< The name and main identifier of the column as reported by its parent
	Module *p;  //!< A pointer to the column's parent Module
	
	Column(QString name, Module *parent) {
		n = name;
		p = parent;
	}
	
	QByteArray* buffer() {return &c;}
};

/*!
 * \brief DataDef struct
 * 
 * An ordered representation of Columns, which can model the format of data at
 * any point in the stream.
 */
typedef QList<Column*> DataDef;

typedef QList<Module*> ModuleList;

#endif // DATA_H
