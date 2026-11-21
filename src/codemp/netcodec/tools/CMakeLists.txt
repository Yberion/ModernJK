#============================================================================
# Copyright (C) 2026, ModernJK contributors
#
# This file is part of the ModernJK source code.
#
# ModernJK is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#============================================================================

# The format's tools: netcodec_train trains the tables on the corpus traces (export_demo_traces.py) and writes
# src/generated/jka_tables.inc; netcodec_train_client trains the client packets' tables on the client traces
# (netcodec_bench_client --trace) and writes src/generated/jka_client_tables.inc.
foreach(tool train train_client)
	netcodec_add_program(netcodec_${tool} ${tool}.cpp)
	target_link_libraries(netcodec_${tool} PRIVATE netcodec_traces netcodec_internal)
endforeach()
