<?php

/**
 * Produce a total score.
 *
 * $Id$
 */
 
require('init.php');
$title="Scoreboard";
require('../header.php');

$contdata = $DB->q('TUPLE SELECT * FROM contest ORDER BY starttime DESC LIMIT 1');
$cid = $contdata['cid'];
?>

<h1>Scoreboard <?=htmlentities($contdata['contestname'])?></h1>


<table border="1">
<?php
$teams = $DB->q('TABLE SELECT login,name,category
	FROM team');
$probs = $DB->q('TABLE SELECT probid,name
	FROM problem WHERE allow_submit = 1 ORDER BY probid');

echo "<tr><th>TEAM</th>";
echo "<th>#correct</th><th>time</th>\n";
foreach($probs as $pr) {
	echo "<th title=\"".htmlentities($pr['name'])."\">".htmlentities($pr['probid'])."</th>";
}
echo "</tr>\n";

$THEMATRIX = $SCORES = $TEAMNAMES = array();

foreach($teams as $team) {

	// to lookup the team name at the end
	$TEAMNAMES[$team['login']]=$team['name'];

	// reset vars
	$grand_total_correct = 0;
	$grand_total_time = 0;
	
	foreach($probs as $pr) {

		$result = $DB->q('SELECT result, 
			(UNIX_TIMESTAMP(submittime)-UNIX_TIMESTAMP(c.starttime))/60 as timediff
			FROM judging LEFT JOIN submission s USING(submitid)
			LEFT OUTER JOIN contest c ON(c.cid=s.cid)
			WHERE team = %s AND probid = %s AND valid = 1 AND result IS NOT NULL AND s.cid = %i
			ORDER BY submittime',
			$team['login'], $pr['probid'], $cid);

		// reset vars
		$total_submitted = $penalty = $total_time = 0;
		$correct = FALSE;

		// for each submission
		while($row = $result->next()) {
			$total_submitted++;

			// if correct, don't look at any more submissions after this one
			if($row['result'] == 'correct') {

				$correct = TRUE;
				$total_time = round((int)@$row['timediff']);
				
				break;
			}

			// 20 penality minutes for each submission
			// (will only be counted if this problem is correctly solved)
			$penalty += 20;
		}

		// calculate penalty time: only when correct add it to the total
		if(!$correct) {
			$penalty = 0;
		} else {
			$grand_total_correct++;
			$grand_total_time += ($total_time + $penalty);
		}

		// THEMATRIX contains the scores for each problem.
		$THEMATRIX[$team['login']][$pr['probid']] = array (
			'correct' => $correct,
			'submitted' => $total_submitted,
			'time' => $total_time,
			'penalty' => $penalty );

	}

	// SCORES contains the total number correct and time for each team.
	// This is our sorting criterion and alpabetically on teamname otherwise. 
	$SCORES[$team['login']]['num_correct'] = $grand_total_correct;
	$SCORES[$team['login']]['total_time'] = $grand_total_time;
	$SCORES[$team['login']]['teamname'] = $TEAMNAMES[$team['login']];

}

// sort the array using our custom comparison function
uasort($SCORES, 'cmp');

// print the whole thing
foreach($SCORES as $team => $totals) {

	// team name, total correct, total time
	echo "<tr><td>".htmlentities($TEAMNAMES[$team])
		."</td><td>"
		.$totals['num_correct']."</td><td>".$totals['total_time']."</td>";
	// for each problem
	foreach($THEMATRIX[$team] as $prob => $pdata) {
		echo "<td class=\"";
		// CSS class for correct/incorrect/neutral results
		if( $pdata['correct'] ) { 
			echo 'correct';
		} elseif ( $pdata['submitted'] > 0 ) {
			echo 'incorrect';
		} else {
			echo 'neutral';
		}
		// number of submissions for this problem
		echo "\">" . $pdata['submitted'];
		// if correct, print time scored
		if( ($pdata['time']+$pdata['penalty']) > 0) {
			echo " (".($pdata['time']+$pdata['penalty']).")";
		}
		echo "</td>";
	}
	echo "</tr>\n";

}

echo "</table>\n\n";


// last modified date
echo "<div id=\"lastmod\">Last Update: " . date('j M Y H:i') . "</div>\n\n";

require('../footer.php');

// comparison function
function cmp ($a, $b) {
	if ( $a['num_correct'] != $b['num_correct'] ) {
		return $a['num_correct'] > $b['num_correct'] ? -1 : 1;
	}
	if ( $a['total_time'] != $b['total_time'] ) {
		return $a['total_time'] < $b['total_time'] ? -1 : 1;
	}
	if ( $a['teamname'] != $b['teamname'] ) {
		return $a['teamname'] < $b['teamname'] ? -1 : 1;
	}
	return 0;
}
