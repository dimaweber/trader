alter table currencies add unique (name);
alter table order_status add unique(status);
insert into version(major, minor) values (2, 1);
