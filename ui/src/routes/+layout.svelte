<script lang="ts">
	import './layout.css';

	let { children } = $props();
</script>

<svelte:head>
	<title>Cacophonic Clip</title>
</svelte:head>

{@render children()}
