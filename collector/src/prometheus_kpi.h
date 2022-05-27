/*
 * prometheus_kpi.h
 * Developer: Satyabrata Bharati.
 * (C) Copyright 2022 Francesco Montorsi

* Description:
* Defines PrometheusKpi class which is an abstract base class for PrometheusCounter
* class which creates Counter Family Object for each KPI with type "COUNTER" and
* PrometheusGauge class which creates Gauge Family Object for each KPI with type "GAUGE".

* This program is free software: you can redistribute it and/or modify
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

#ifndef _PROMETHEUS_KPI_H_
#define _PROMETHEUS_KPI_H_

#include <prometheus/metric_type.h>

typedef prometheus::MetricType KPI_TYPE;

typedef struct {
    std::string kpi_name;
    KPI_TYPE kpi_type;
    std::string description;
} prometheus_kpi_descriptor;

class PrometheusKpi {
public:
    PrometheusKpi() = default;
    PrometheusKpi(const PrometheusKpi&) = delete; // Copy constructor
    PrometheusKpi(PrometheusKpi&&) = delete; // Move constructor
    PrometheusKpi& operator=(const PrometheusKpi&) = delete; // Copy assignment operator
    PrometheusKpi& operator=(PrometheusKpi&&) = delete; // Move assignment operator

    virtual ~PrometheusKpi() = default;

    virtual void SetKpiValue(double kpi_value) = 0;
    virtual void SetKpiValue(double kpi_value, std::map<std::string, std::string>& labels) = 0;
};

#endif
