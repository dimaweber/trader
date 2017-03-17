-- deposit per day
select  date(A.time) as date,
		ROUND(AVG(A.sum+B.sum),2) as usd
from (select TO_DAYS(dep.time) as days,
			 dep.time as time,
			 sum((dep.on_orders + dep.value) * rates.last_rate) as sum
	  from dep
		left join rates on dep.name=rates.goods and dep.time=rates.time
	  where rates.time is not null and rates.currency='usd'
	  group by dep.time) as A
		left join (select dep.time as time, dep.value + dep.on_orders as sum from dep where name='usd') as B on A.time=B.time
group by A.days;

-- current deposit
SELECT
	A.time AS date,
	ROUND(A.sum, 2) AS equiv,
	ROUND(B.sum, 2) AS currency,
	ROUND(A.sum + B.sum, 2) AS total
FROM
	(SELECT
		TO_DAYS(dep.time) AS days,
			dep.time AS time,
			SUM((dep.on_orders + dep.value) * rates.last_rate) AS sum
	FROM
		dep
	LEFT JOIN rates ON dep.name = rates.goods
		AND dep.time = rates.time
	WHERE
		date(rates.time) = date(now())
			AND rates.currency = 'usd'
	GROUP BY dep.time) AS A
		LEFT JOIN
	(SELECT
		dep.time AS time, dep.value + dep.on_orders AS sum
	FROM
		dep
	WHERE
		name = 'usd'
			AND date(dep.time) = date(now())) AS B ON A.time = B.time
ORDER BY A.time DESC
LIMIT 1;
