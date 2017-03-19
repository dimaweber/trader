alter table rounds modify reason enum('active', 'archive', 'done') not null default 'active';
create temporary table T as select  max(round_id) from rounds where reason='done' group by settings_id;
update rounds set reason='archive' where round_id in (select * from T);
drop table T;
insert into version(major, minor) values (2, 3);
