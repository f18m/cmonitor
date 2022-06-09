/*
 * prometheus_counter.cpp -- code for collecting prometheus counter metrics
 * Developer: Satyabrata Bharati.
 * (C) Copyright 2022 Francesco Montorsi
 *
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifdef PROMETHEUS_SUPPORT
#include "prometheus_counter.h"

PrometheusCounter::PrometheusCounter(std::shared_ptr<prometheus::Registry> prometheus_registry, std::string kpi_name,
    std::string kpi_description, std::map<std::string, std::string>& labels)
    : m_prometheus_kpi_family(
        prometheus::BuildCounter().Name(kpi_name).Help(kpi_description).Labels(labels).Register(*prometheus_registry))
    , m_prometheus_kpi(m_prometheus_kpi_family.Add({}))

{
    m_prometheus_kpi_name = kpi_name;
}

void PrometheusCounter::set_kpi_value(double kpi_value)
{
    double previous_kpi_value = m_prometheus_kpi.Value();
    if (kpi_value < previous_kpi_value) {
        CMonitorLogger::instance()->LogError(
            "PrometheusCounter::set_kpi_value, the current KPI value=%ld is less than the previous KPI value=%ld",
            (uint64_t)kpi_value, (uint64_t)previous_kpi_value);
        return;
    } else if (kpi_value > previous_kpi_value) {
        m_prometheus_kpi.Increment(kpi_value - previous_kpi_value);
    }
}

void PrometheusCounter::set_kpi_value(double kpi_value, const std::map<std::string, std::string>& labels)
{
    prometheus::Counter& prometheus_kpi = m_prometheus_kpi_family.Add(labels);

    double previous_kpi_value = prometheus_kpi.Value();
    if (kpi_value < previous_kpi_value) {
        CMonitorLogger::instance()->LogError("PrometheusCounter::set_kpi_value with kpi_name=%s, the current KPI value "
                                             "= %ld is less than the previous KPI "
                                             "value = %ld",
            m_prometheus_kpi_name.c_str(), (uint64_t)kpi_value, (uint64_t)previous_kpi_value);
        return;
    } else if (kpi_value > previous_kpi_value) {
        prometheus_kpi.Increment(kpi_value - previous_kpi_value);
    }
}
#endif