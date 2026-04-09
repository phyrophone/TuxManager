/*
 * Tux Manager - Linux system monitor
 * Copyright (C) 2026 Petr Bena <petr@bena.rocks>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef KERNEL_H
#define KERNEL_H

class Kernel
{
    public:
        Kernel();

        bool Sample();

        int ProcessCount() const { return this->m_processCount; }
        int ThreadCount()  const { return this->m_threadCount;  }

    private:
        /// Count processes/threads via /proc walk for CPU detail statistics.
        void sampleProcessStats();

        // Process/thread counts
        int      m_processCount { 0 };
        int      m_threadCount  { 0 };
};

#endif // KERNEL_H
