select  date(A.time) as date,
        ROUND(AVG(A.sum),2) as equiv,
        ROUND(AVG(B.sum),2) as currency,
        ROUND(AVG(A.sum+B.sum),2) as total
from (select TO_DAYS(dep.time) as days,
             dep.time as time,
             sum((dep.on_orders + dep.value) * rates.last_rate) as sum
      from dep
        left join rates on dep.name=rates.goods and dep.time=rates.time
      where rates.time is not null and rates.currency='usd'
      group by dep.time) as A
        left join (select dep.time as time, dep.value + dep.on_orders as sum from dep where name='usd') as B on A.time=B.time
group by A.days;
