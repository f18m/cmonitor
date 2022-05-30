/*
 * prometheus_gauge.cpp -- code for collecting prometheus gauge metrics
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

#include "prometheus_gauge.h"

PrometheusGauge::PrometheusGauge(std::shared_ptr<prometheus::Registry> prometheus_registry, std::string kpi_name,
    std::string kpi_description, std::map<std::string, std::string>& labels)
    : m_prometheus_kpi_family(
        prometheus::BuildGauge().Name(kpi_name).Help(kpi_description).Labels(labels).Register(*prometheus_registry))
    , m_prometheus_kpi(m_prometheus_kpi_family.Add({}))

{
}

void PrometheusGauge::set_kpi_value(double kpi_value) { m_prometheus_kpi.Set(kpi_value); }

void PrometheusGauge::set_kpi_value(double kpi_value, std::map<std::string, std::string>& labels)
{
    prometheus::Gauge& prometheus_kpi = m_prometheus_kpi_family.Add(labels);

    prometheus_kpi.Set(kpi_value);
}
