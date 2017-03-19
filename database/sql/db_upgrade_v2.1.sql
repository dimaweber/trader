insert into order_status (status_id, status) values (-2, 'transition'), (4, "instant");
create index transactions_order_id_idx on transactions(order_id);
create temporary table tmp_instant_update as select o.order_id from transactions t right join orders o on o.order_id=t.order_id where t.id is null and o.status_id <> 4;
update orders set status_id=4, modified=modified where order_id in (select * from tmp_instant_update);
drop table tmp_instant_update;
insert into version(major, minor) values (2, 2);
