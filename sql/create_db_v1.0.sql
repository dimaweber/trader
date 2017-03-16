drop database trade_debug;
create database trade_debug;
grant all on trade_debug.* to 'trader'@'%.dweber.lan' identified by 'traderdebug';
use trade_debug;

CREATE TABLE `dep` (
  `time` datetime NOT NULL,
  `name` char(3) NOT NULL,
  `secret_id` int(11) NOT NULL,
  `value` decimal(14,6) NOT NULL,
  `on_orders` decimal(14,6) NOT NULL DEFAULT '0.000000',
  UNIQUE KEY `uniq_rate` (`time`,`name`,`secret_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `orders` (
  `order_id` int(11) NOT NULL,
  `status` int(11) NOT NULL DEFAULT '0',
  `type` char(4) NOT NULL DEFAULT 'buy',
  `amount` decimal(11,6) DEFAULT NULL,
  `rate` decimal(11,6) DEFAULT NULL,
  `settings_id` int(11) DEFAULT NULL,
  `backed_up` int(11) NOT NULL DEFAULT '0',
  `start_amount` decimal(11,6) DEFAULT NULL,
  `round_id` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`order_id`),
  KEY `orders_settings_id_idx` (`settings_id`),
  KEY `orders_round_id_idx` (`round_id`),
  KEY `orders_type_idx` (`type`),
  KEY `orders_status_idx` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

CREATE TABLE `rates` (
  `time` datetime NOT NULL,
  `currency` char(3) NOT NULL,
  `goods` char(3) NOT NULL,
  `buy_rate` decimal(14,6) NOT NULL,
  `sell_rate` decimal(14,6) NOT NULL,
  `last_rate` decimal(14,6) NOT NULL,
  `currency_volume` decimal(14,6) NOT NULL,
  `goods_volume` decimal(14,6) NOT NULL,
  UNIQUE KEY `uniq_rate` (`time`,`currency`,`goods`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `rounds` (
  `round_id` int(11) NOT NULL AUTO_INCREMENT,
  `settings_id` int(11) NOT NULL,
  `start_time` datetime NOT NULL,
  `end_time` datetime DEFAULT NULL,
  `income` decimal(14,6) DEFAULT NULL,
  `reason` char(16) COLLATE utf8_unicode_ci NOT NULL DEFAULT 'active',
  `g_in` decimal(14,6) NOT NULL DEFAULT '0.000000',
  `g_out` decimal(14,6) NOT NULL DEFAULT '0.000000',
  `c_in` decimal(14,6) NOT NULL DEFAULT '0.000000',
  `c_out` decimal(14,6) NOT NULL DEFAULT '0.000000',
  `dep_usage` decimal(14,6) NOT NULL DEFAULT '0.000000',
  PRIMARY KEY (`round_id`),
  KEY `rounds_settings_type_idx` (`settings_id`)
) ENGINE=InnoDB AUTO_INCREMENT=567 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

CREATE TABLE `secrets` (
  `apikey` char(255) NOT NULL,
  `secret` char(255) NOT NULL,
  `id` int(11) NOT NULL,
  `is_crypted` tinyint(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

CREATE TABLE `settings` (
  `id` int(11) NOT NULL,
  `profit` decimal(6,4) NOT NULL DEFAULT '0.0100',
  `comission` decimal(6,4) NOT NULL DEFAULT '0.0020',
  `first_step` decimal(6,4) NOT NULL DEFAULT '0.0500',
  `martingale` decimal(6,4) NOT NULL DEFAULT '0.0500',
  `dep` decimal(10,4) NOT NULL DEFAULT '100.0000',
  `coverage` decimal(6,4) NOT NULL DEFAULT '0.1500',
  `count` int(11) NOT NULL DEFAULT '10',
  `currency` char(3) NOT NULL DEFAULT 'usd',
  `goods` char(3) NOT NULL DEFAULT 'btc',
  `secret_id` int(11) NOT NULL,
  `dep_inc` decimal(5,2) NOT NULL DEFAULT '0.00',
  `enabled` tinyint(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

CREATE TABLE `transactions` (
  `id` bigint(20) DEFAULT NULL,
  `type` int(11) NOT NULL,
  `amount` double NOT NULL,
  `currency` char(3) NOT NULL,
  `description` varchar(255) DEFAULT NULL,
  `status` int(11) DEFAULT NULL,
  `secret_id` int(11) DEFAULT NULL,
  `timestamp` datetime NOT NULL,
  `order_id` bigint(20) NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

insert into secrets (id, apikey, secret, is_crypted) values
(1, 'd62263e47cc1fd7c7bcf6e4b4f2e84d2ed87c78dd3438240c0268833b18c8face12d835c35ba1ff020da6aaa',
'5dd80c19c7b7476d5031a3b03dee086ca5c4062c7d5cd983b62e18f71705f2d50ee8b1477555401cfee24965135f563618d58b87241f7a8a28efce3f36ae17aa', 1);

insert into settings values (1, 0.01, 0.002, .01, .05, 12.9572, .1, 5, 'usd', 'eth', 1.00, 1, 1);

insert into rounds values
(25, 1, '2017-03-16 00:47:16', '2017-03-16 01:41:30', 0.043060,   'sell', 0.128681,  0.128681, 4.341249, 4.298189, 12.914100),
(26, 1, '2017-03-16 01:41:31', NULL                 ,        0, 'active',        0,         0,        0,        0,         0);


insert into orders (order_id, status,type, amount, rate, start_amount, round_id, settings_id)
values (1652761004, 1, 'buy', 0.000000, 33.335060,    0.128939 ,       25 , 1),
( 1652819302 ,         1 , 'sell' , 0.000000 , 33.804130 ,     0.128681 ,       25 , 1),
( 1652918653 ,         0 , 'buy'  , 0.121323 , 35.545950 ,     0.121323 ,       26 , 1),
( 1652918661 ,         0 , 'buy'  , 0.127389 , 33.930230 ,     0.127389 ,       26 , 1),
( 1652918667 ,         0 , 'buy'  , 0.133758 , 32.314500 ,     0.133758 ,       26 , 1);

