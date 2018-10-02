CREATE TABLE `character_squelch` (
	`character_id` INT(11) UNSIGNED NOT NULL DEFAULT '0',
	`squelched_id` INT(11) UNSIGNED NOT NULL DEFAULT '0',
	`account_id` INT(11) UNSIGNED NOT NULL DEFAULT '0',
	`isip` BIT(1) NULL DEFAULT b'0',
	`isspeech` BIT(1) NULL DEFAULT b'0',
	`istell` BIT(1) NULL DEFAULT b'0',
	`iscombat` BIT(1) NULL DEFAULT b'0',
	`ismagic` BIT(1) NULL DEFAULT b'0',
	`isemote` BIT(1) NULL DEFAULT b'0',
	`isadvancement` BIT(1) NULL DEFAULT b'0',
	`isappraisal` BIT(1) NULL DEFAULT b'0',
	`isspellcasting` BIT(1) NULL DEFAULT b'0',
	`isallegiance` BIT(1) NULL DEFAULT b'0',
	`isfellowhip` BIT(1) NULL DEFAULT b'0',
	`iscombatenemy` BIT(1) NULL DEFAULT b'0',
	`isrecall` BIT(1) NULL DEFAULT b'0',
	`iscrafting` BIT(1) NULL DEFAULT b'0',
	PRIMARY KEY (`character_id`, `squelched_id`)
)
COLLATE='utf8_general_ci'
ENGINE=InnoDB
;